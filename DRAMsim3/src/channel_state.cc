#include "channel_state.h"
#include "fmt/format.h"

namespace dramsim3 {
ChannelState::ChannelState(const Config& config, const Timing& timing, SimpleStats& simple_stats, int channel)
    : rank_idle_cycles(config.ranks, 0),
      config_(config),
      timing_(timing),
      simple_stats_(simple_stats),
      channel_(channel),
      rank_is_sref_(config.ranks, false),
      four_aw_(config_.ranks, std::vector<uint64_t>()),
      thirty_two_aw_(config_.ranks, std::vector<uint64_t>()) {
    
    bank_states_.reserve(config_.ranks);

    for (auto i = 0; i < config_.ranks; i++)
    {
        auto rank_states = std::vector<std::vector<BankState>>();
        rank_states.reserve(config_.bankgroups);
        for (auto j = 0; j < config_.bankgroups; j++)
        {
            std::vector<BankState> bg_states;
            for (auto k = 0; k < config_.banks_per_group; k++)
            {
                bg_states.push_back(BankState(config_, simple_stats_, i, j, k));
            }
            rank_states.push_back(bg_states);
        }
        bank_states_.push_back(rank_states);
    }

    // [Hydra]
    hydra_wb_draining_ = false;

    // [ABO]
    alert_n = false;
    last_alert_clk_ = 0;
    num_acts_abo_ = 0;

    // [REF]
    ref_idx_ = 0;
    fgr_counter_ = 0;

    // [Tracking]
    last_bus_access_time_ = 0;
    bursty_access_count_ = 0;

    // [DREAM]
    tusc_.resize(config_.rows / config_.dream_k, 0);
    tusc_prev_.resize(config_.rows / config_.dream_k, 0);
    tusc_size_ = tusc_.size();
    for (uint32_t i = 0; i < config_.ranks * config_.bankgroups * config_.banks_per_group * config_.dream_k; i++)
    {
        random_masks.push_back(rand() % tusc_size_);
    }

    if (config_.dream_mode == 1)
    {
        dream_resets_stat_ = "dream_resets";
        simple_stats_.InitStat(dream_resets_stat_, "counter", "dream Resets Counter");
    }
    else if (config_.abacus_mode == 1)
    {
        // uint64_t max_acts = (config_.refchunks * (config_.tREFI - config_.tRFC)) / (config_.tRAS + config_.tRP);
        abacus_entries_ = config_.rows;
        std::cout << "ABACUS Entries: " << abacus_entries_ << std::endl;
        abacus_table_.resize(abacus_entries_);
        
        abacus_resets_stat_ = "abacus_resets";
        simple_stats_.InitStat(abacus_resets_stat_, "counter", "abacus Resets Counter");
    }
}

uint32_t ChannelState::get_tusc_idx(uint32_t rank, uint32_t bankgroup, uint32_t bank, uint32_t rowid) const 
{
    uint32_t groupid = rowid / config_.dream_k;
    uint32_t bank_idx = rank * config_.bankgroups * config_.banks_per_group + bankgroup * config_.banks_per_group + bank;
    uint32_t total_banks = config_.ranks * config_.bankgroups * config_.banks_per_group;
    uint32_t row_num = rowid % config_.dream_k;

    if (config_.dream_policy == 0) // Set-Associative
    {
        return groupid;
    }
    
    if (config_.dream_policy == 1) // Staggered
    {
        rowid = rowid % tusc_size_;
        uint32_t skewed_idx = (rowid - bank_idx + tusc_size_) % tusc_size_;
        return skewed_idx;
    }

    if (config_.dream_policy == 2) // Random
    {
        return (groupid ^ random_masks[bank_idx + row_num * total_banks]);
    }
    
    AbruptExit(__FILE__, __LINE__);
}

uint32_t ChannelState::get_row_idx(uint32_t rank, uint32_t bankgroup, uint32_t bank, uint32_t tusc_idx, uint32_t row_num) const 
{
    uint32_t bank_idx = rank * config_.bankgroups * config_.banks_per_group + bankgroup * config_.banks_per_group + bank;
    uint32_t total_banks = config_.ranks * config_.bankgroups * config_.banks_per_group;

    if (config_.dream_policy == 0) // Set-Associative
    {
        return tusc_idx * config_.dream_k + row_num;
    }
    
    if (config_.dream_policy == 1) // Staggered
    {
        uint32_t rowid = (tusc_idx + bank_idx + row_num * tusc_size_) % config_.rows;
        return rowid;
    }
    
    if (config_.dream_policy == 2) // Random
    {
        uint32_t rowid = (tusc_idx ^ random_masks[bank_idx + row_num * total_banks]) * config_.dream_k + row_num;
        return rowid;
    }
    
    AbruptExit(__FILE__, __LINE__);
}

void ChannelState::dream_preact(uint32_t rank, uint32_t bankgroup, uint32_t bank, uint32_t rowid) 
{
    uint32_t tusc_idx = get_tusc_idx(rank, bankgroup, bank, rowid);
    tusc_[tusc_idx]++;

    if (config_.dream_mode == 0) return;

    uint32_t counter_val = tusc_[tusc_idx];
    uint32_t threshold = config_.dream_th;

    if (config_.dream_prev_enable)
    {
        counter_val += tusc_prev_[tusc_idx];
        threshold *= 2;
    }

    if (counter_val >= threshold)
    {
        // insert DRFM entry to all the banks
        for (int i = 0; i < config_.ranks; i++)
        {
            for (int j = 0; j < config_.bankgroups; j++)
            {
                for (int k = 0; k < config_.banks_per_group; k++)
                {
                    for (int l = 0; l < config_.dream_k; l++)
                    {
                        uint32_t _rowid = get_row_idx(i, j, k, tusc_idx, l);
                        bank_states_[i][j][k].insert_drfm(_rowid);
                    }
                }
            }
        }

        // push the same tusc_idx to the queue
        for (int l = 0; l < config_.dream_k; l++)
        {
            tusc_q_.push_back(tusc_idx);
        }
    }
}

void ChannelState::abacus_preact(uint32_t rank, uint32_t bankgroup, uint32_t bank, uint32_t rowid) 
{
    if (config_.abacus_mode == 0) return;

    uint32_t bank_idx = rank * config_.bankgroups * config_.banks_per_group + bankgroup * config_.banks_per_group + bank;
    uint64_t bank_mask = 0x1ull << bank_idx;

    if ((abacus_table_[rowid].sav & bank_mask) == 0)
    {
        abacus_table_[rowid].sav |= bank_mask;
    }
    else
    {
        abacus_table_[rowid].rac++;
        abacus_table_[rowid].sav = bank_mask;
    }

    if (abacus_table_[rowid].rac >= config_.abacus_th)
    {
        // insert DRFM entry to all the banks
        for (int i = 0; i < config_.ranks; i++)
        {
            for (int j = 0; j < config_.bankgroups; j++)
            {
                for (int k = 0; k < config_.banks_per_group; k++)
                {
                    bank_states_[i][j][k].insert_drfm(rowid);
                }
            }
        }

        abacus_q_.push_back(rowid);
    }

}

void ChannelState::dream_mitig() 
{
    if (config_.dream_mode == 0) return;

    if (tusc_q_.empty()) return;

    uint32_t tusc_idx = tusc_q_.front();
    tusc_prev_[tusc_idx] = tusc_[tusc_idx];
    tusc_[tusc_idx] = 0;
    tusc_q_.erase(tusc_q_.begin());
}

void ChannelState::abacus_mitig() 
{
    if (config_.abacus_mode == 0) return;

    if (abacus_q_.empty()) return;

    uint32_t rowid = abacus_q_.front();
    abacus_table_[rowid].rac = 0;
    abacus_table_[rowid].sav = 0;
    abacus_q_.erase(abacus_q_.begin());
}

void ChannelState::dream_refresh()
{    
    uint32_t factor = config_.dream_reset;

    // Just to dump stats
    if (config_.dream_mode == 0 and ref_idx_ % (config_.refchunks / factor) == 0 and fgr_counter_ == 0)
    {
        // Get quantiles from tusc_
        std::sort(tusc_.begin(), tusc_.end());
        uint32_t min = tusc_[0];
        uint32_t q25 = tusc_[tusc_size_ / 4];
        uint32_t q50 = tusc_[tusc_size_ / 2];
        uint32_t q75 = tusc_[tusc_size_ * 3 / 4];
        uint32_t q90 = tusc_[tusc_size_ * 9 / 10];
        uint32_t q99 = tusc_[tusc_size_ * 99 / 100];
        uint32_t max = tusc_[tusc_size_ - 1];

        double sum = std::accumulate(tusc_.begin(), tusc_.end(), 0.0);
        double mean = sum / tusc_.size();
        std::vector<double> diff(tusc_.size());
        std::transform(tusc_.begin(), tusc_.end(), diff.begin(), [mean](double x) { return x - mean; });
        double sq_sum = std::inner_product(diff.begin(), diff.end(), diff.begin(), 0.0);
        double stdev = std::sqrt(sq_sum / tusc_.size());

        fmt::print("\n[{}][{}] mean: {} stddev: {} min: {} q25: {} q50: {} q75: {} q90: {} q99: {} max: {}\n", 
            channel_, ref_idx_, mean, stdev, min, q25, q50, q75, q90, q99, max);

        tusc_.clear();
        tusc_.resize(tusc_size_, 0);
    }
    else if (config_.dream_mode == 1 and fgr_counter_ == 7)
    {
        simple_stats_.Increment(dream_resets_stat_);

        uint32_t factored_ref_idx = ref_idx_ % (config_.refchunks / factor);
        uint32_t tusc_rows_per_ref = factor * (tusc_size_ / config_.refchunks);
        for (int i = 0; i < tusc_rows_per_ref; i++)
        {
            uint32_t index = factored_ref_idx * tusc_rows_per_ref + i;
            tusc_prev_[index] = tusc_[index]; 
            tusc_[index] = 0;
        }
    }
}

void ChannelState::abacus_refresh()
{
    if (config_.abacus_mode == 0) return;

    if (fgr_counter_ == 7)
    {
        uint32_t start = ref_idx_ % config_.refchunks;
        uint32_t abacus_rows_per_ref = abacus_entries_ / config_.refchunks;

        for (int i = 0; i < abacus_rows_per_ref; i++)
        {
            uint32_t rowid = start * abacus_rows_per_ref + i;
            abacus_table_[rowid].rac = 0;
            abacus_table_[rowid].sav = 0;
        }

        simple_stats_.Increment(abacus_resets_stat_);
    }
}



void ChannelState::PrintDeadlock() const {
    for (auto i = 0; i < config_.ranks; i++) {
        for (auto j = 0; j < config_.bankgroups; j++) {
            for (auto k = 0; k < config_.banks_per_group; k++) {
                printf("Rank %d Bankgroup %d Bank %d\n", i, j, k);
                bank_states_[i][j][k].PrintState();
                printf("====================================\n");
            }
        }
    }
    return;
}

bool ChannelState::IsAllBankIdleInRank(int rank) const {
    for (int j = 0; j < config_.bankgroups; j++) {
        for (int k = 0; k < config_.banks_per_group; k++) {
            if (bank_states_[rank][j][k].IsRowOpen()) {
                return false;
            }
        }
    }
    return true;
}

bool ChannelState::IsRWPendingOnRef(const Command& cmd) const {
    int rank = cmd.Rank();
    int bankgroup = cmd.Bankgroup();
    int bank = cmd.Bank();
    return (IsRowOpen(rank, bankgroup, bank) &&
            RowHitCount(rank, bankgroup, bank) == 0 &&
            bank_states_[rank][bankgroup][bank].OpenRow() == cmd.Row());
}

bool ChannelState::HydraRead(int rank, int bankgroup, int bank, int row) {
    // if (channel_ == 0)
    // printf("HydraRead: channel %d rank %d bankgroup %d bank %d row %d\n", channel_, rank, bankgroup, bank, row);
    // check if request is already present in the writeback queue
    bool present = false;
    for (auto it = hydra_wb_q_.begin(); it != hydra_wb_q_.end(); it++) {
        if (it->Rank() == rank && it->Bankgroup() == bankgroup &&
            it->Bank() == bank && it->Row() == row) {
            present = true;
            break;
        }
    }

    if (present)
    {
        // std::cout << "HydraRead: Forwading from WBQ" << std::endl;
        return true;
    }

    // check if request is already present in the read queue
    for (auto it = hydra_rd_q_.begin(); it != hydra_rd_q_.end(); it++) {
        if (it->Rank() == rank && it->Bankgroup() == bankgroup &&
            it->Bank() == bank && it->Row() == row) {
            return false;
        }
    }

    Address addr = Address(channel_, rank, bankgroup, bank, row, -1);
    hydra_rd_q_.emplace_back(CommandType::READ, addr, -1);
    return false;
}

void ChannelState::HydraWB(int rank, int bankgroup, int bank, int row) {

    // check if request is already present in the writeback queue
    for (auto it = hydra_wb_q_.begin(); it != hydra_wb_q_.end(); it++) {
        if (it->Rank() == rank && it->Bankgroup() == bankgroup &&
            it->Bank() == bank && it->Row() == row) {
            return;
        }
    }

    Address addr = Address(channel_, rank, bankgroup, bank, row, -1);
    hydra_wb_q_.emplace_back(CommandType::WRITE, addr, -1);

    // std::cout << "[" << channel_ << "] HydraWB: WBQ Size: " << hydra_wb_q_.size() << std::endl;
}

void ChannelState::BankNeedRefresh(int rank, int bankgroup, int bank,
                                   bool need) {
    if (need) {
        Address addr = Address(-1, rank, bankgroup, bank, -1, -1);
        refresh_q_.emplace_back(CommandType::REFRESH_BANK, addr, -1);
    } else {
        for (auto it = refresh_q_.begin(); it != refresh_q_.end(); it++) {
            if (it->Rank() == rank && it->Bankgroup() == bankgroup &&
                it->Bank() == bank) {
                refresh_q_.erase(it);
                break;
            }
        }
    }
    return;
}

void ChannelState::BanksetNeedRefresh(int rank, int bank, bool need) {
    if (need) {
        Address addr = Address(-1, rank, -1, bank, -1, -1);
        refresh_q_.emplace_back(CommandType::REFsb, addr, -1);
    } else {
        for (auto it = refresh_q_.begin(); it != refresh_q_.end(); it++) {
            if (it->Rank() == rank && it->Bank() == bank) {
                refresh_q_.erase(it);
                break;
            }
        }
    }
    return;
}

void ChannelState::RankNeedRefresh(int rank, bool need) {
    if (need) {
        Address addr = Address(-1, rank, -1, -1, -1, -1);
        refresh_q_.emplace_back(CommandType::REFab, addr, -1);
    } else {
        for (auto it = refresh_q_.begin(); it != refresh_q_.end(); it++) {
            if (it->Rank() == rank) {
                refresh_q_.erase(it);
                break;
            }
        }
    }
    return;
}


void ChannelState::BanksetNeedRFM(int rank, int bank, bool need) {
    // Check if the rank is already present in the queue
    bool bankset_present = false;
    for (const auto& cmd : rfm_q_)
    {
        if (cmd.Rank() == rank and cmd.Bank() == bank)
        {
            bankset_present = true;
            break;
        }
    }

    if (need and !bankset_present)
    {
        Address addr = Address(-1, rank, -1, bank, -1, -1);
        rfm_q_.emplace_back(CommandType::RFMsb, addr, -1);
    }
    else if (!need)
    {
        // Remove the rank from the queue if it is present
        for (auto it = rfm_q_.begin(); it != rfm_q_.end(); ++it) {
            if (it->Rank() == rank and bank == it->Bank()) {
                rfm_q_.erase(it);
                break;
            }
        }
    }
    return;
}

void ChannelState::RankNeedRFM(int rank, bool need) {
    // Check if the rank is already present in the queue
    bool rank_present = false;
    for (const auto& cmd : rfm_q_)
    {
        if (cmd.Rank() == rank)
        {
            rank_present = true;
            break;
        }
    }

    if (need and !rank_present)
    {
        Address addr = Address(-1, rank, -1, -1, -1, -1);
        rfm_q_.emplace_back(CommandType::RFMab, addr, -1);
    }
    else if (!need)
    {
        // Remove the rank from the queue if it is present
        for (auto it = rfm_q_.begin(); it != rfm_q_.end(); ++it) {
            if (it->Rank() == rank) {
                rfm_q_.erase(it);
                break;
            }
        }
    }
    return;
}

void ChannelState::BankNeedDRFM(int rank, int bankgroup, int bank, bool need) {
    bool bank_present = false;
    for (const auto& cmd : rfm_q_)
    {
        if (cmd.Rank() == rank and cmd.Bankgroup() == bankgroup and cmd.Bank() == bank)
        {
            bank_present = true;
            break;
        }
    }

    if (need and !bank_present)
    {
        Address addr = Address(-1, rank, bankgroup, bank, -1, -1);
        rfm_q_.emplace_back(CommandType::DRFMb, addr, -1);
    }
    else if (!need)
    {
        // Remove the rank from the queue if it is present
        for (auto it = rfm_q_.begin(); it != rfm_q_.end(); ++it) {
            if (it->Rank() == rank and bankgroup == it->Bankgroup() and bank == it->Bank()) {
                rfm_q_.erase(it);
                break;
            }
        }
    }
    return;
}

void ChannelState::BanksetNeedDRFM(int rank, int bank, bool need) {
    bool bankset_present = false;
    for (const auto& cmd : rfm_q_)
    {
        if (cmd.Rank() == rank and cmd.Bank() == bank)
        {
            bankset_present = true;
            break;
        }
    }

    if (need and !bankset_present)
    {
        Address addr = Address(-1, rank, -1, bank, -1, -1);
        rfm_q_.emplace_back(CommandType::DRFMsb, addr, -1);
    }
    else if (!need)
    {
        // Remove the rank from the queue if it is present
        for (auto it = rfm_q_.begin(); it != rfm_q_.end(); ++it) {
            if (it->Rank() == rank and bank == it->Bank()) {
                rfm_q_.erase(it);
                break;
            }
        }
    }
    return;
}

void ChannelState::RankNeedDRFM(int rank, bool need) {
    // Check if the rank is already present in the queue
    bool rank_present = false;
    for (const auto& cmd : rfm_q_)
    {
        if (cmd.Rank() == rank)
        {
            rank_present = true;
            break;
        }
    }

    if (need and !rank_present)
    {
        Address addr = Address(-1, rank, -1, -1, -1, -1);
        rfm_q_.emplace_back(CommandType::DRFMab, addr, -1);
    }
    else if (!need)
    {
        // Remove the rank from the queue if it is present
        for (auto it = rfm_q_.begin(); it != rfm_q_.end(); ++it) {
            if (it->Rank() == rank) {
                rfm_q_.erase(it);
                break;
            }
        }
    }
    return;
}

Command ChannelState::GetReadyHydraCommand(uint64_t clk) const
{
    if (hydra_wb_q_.size() >= config_.hydra_wbq_size and !hydra_wb_draining_ and hydra_rd_q_.empty())
    {
        const_cast<ChannelState*>(this)->hydra_wb_draining_ = true;
        // if (channel_ == 0)
            // std::cout << "GetReadyHydraCommand: Draining WBQ" << std::endl;
    }

    if (hydra_wb_q_.empty())
    {
        const_cast<ChannelState*>(this)->hydra_wb_draining_ = false;
    }
    
    std::vector<Command>& queue = hydra_wb_draining_ ? const_cast<ChannelState*>(this)->hydra_wb_q_ : const_cast<ChannelState*>(this)->hydra_rd_q_;

    if (queue.empty())
    {
        return Command();
    }

    Command cmd = queue.front();
    Command ready_cmd = bank_states_[cmd.Rank()][cmd.Bankgroup()][cmd.Bank()]
                        .GetReadyCommand(cmd, clk);

    if (ready_cmd.cmd_type == CommandType::ACTIVATE)
    {
        if (!ActivationWindowOk(ready_cmd.Rank(), clk))
        {
            return Command();
        }
    }

    if (ready_cmd.IsValid() and ready_cmd.IsReadWrite())
    {
        // if (channel_ == 0)
        // {

        //     std::cout << "before.size: hydra_rd_q_: " << hydra_rd_q_.size() << " hydra_wb_q_: " << hydra_wb_q_.size() << std::endl;
        //     std::cout << "[" << clk << "] GetReadyHydraCommand: Erasing " << ready_cmd << std::endl;
        // }
        queue.erase(queue.begin());
        // if (channel_ == 0)
        // {
        //     std::cout << "after.size: hydra_rd_q_: " << hydra_rd_q_.size() << " hydra_wb_q_: " << hydra_wb_q_.size() << std::endl;
        // }
    }

    // if (ready_cmd.IsValid() and channel_ == 0)
    // {
    //     std::cout << "[" << clk << "] GetReadyHydraCommand: " << ready_cmd << std::endl;
    // }
    return ready_cmd;
}

Command ChannelState::GetReadyCommand(const Command& cmd, uint64_t clk) const
{
    Command ready_cmd = Command();
    if (cmd.IsRankCMD()) {
        int num_ready = 0;
        for (auto j = 0; j < config_.bankgroups; j++) {
            for (auto k = 0; k < config_.banks_per_group; k++) {
                ready_cmd = bank_states_[cmd.Rank()][j][k].GetReadyCommand(cmd, clk);
                if (!ready_cmd.IsValid()) {  // Not ready
                    continue;
                }
                if (ready_cmd.cmd_type != cmd.cmd_type) {  // likely PRECHARGE
                    Address new_addr = Address(-1, cmd.Rank(), j, k, -1, -1);
                    ready_cmd.addr = new_addr;
                    return ready_cmd;
                } else {
                    num_ready++;
                }
            }
        }
        // All bank ready
        if (num_ready == config_.banks) {
            return ready_cmd;
        } else {
            return Command();
        }
    } 
    else if (cmd.IsSbCMD())
    {
        int num_ready = 0;
        for (auto j = 0; j < config_.bankgroups; j++) {
            ready_cmd = bank_states_[cmd.Rank()][j][cmd.Bank()].GetReadyCommand(cmd, clk);
            if (!ready_cmd.IsValid()) {  // Not ready
                continue;
            }
            if (ready_cmd.cmd_type != cmd.cmd_type) {  // likely PRECHARGE
                Address new_addr = Address(-1, cmd.Rank(), j, cmd.Bank(), -1, -1);
                ready_cmd.addr = new_addr;
                return ready_cmd;
            } else {
                num_ready++;
            }
        }
        // Same bank in all bankgroups ready
        if (num_ready == config_.bankgroups) {
            return ready_cmd;
        } else {
            return Command();
        }
    }
    else if (hydra_wb_draining_ or !hydra_rd_q_.empty())
    {
        return Command();
    }
    else
    {
        ready_cmd = bank_states_[cmd.Rank()][cmd.Bankgroup()][cmd.Bank()]
                        .GetReadyCommand(cmd, clk);
        
        if (!ready_cmd.IsValid()) {
            return Command();
        }

        // if (ready_cmd.Channel() == 0)
        // {
        // std::cout << "===== CLK: " << clk << " =====" << std::endl; 
        // std::cout << "Ready Command: " << ready_cmd << std::endl;
        // std::cout << "CMD: " << cmd << std::endl;
        // std::cout << "Addr: " << std::hex << cmd.hex_addr << std::dec << std::endl;
        // bank_states_[cmd.Rank()][cmd.Bankgroup()][cmd.Bank()].PrintState();
        // }
        // [ABO] allow 180ns of activity before 
        if (alert_n and (clk > (config_.tABO_act + last_alert_clk_)))
        {
            const_cast<ChannelState*>(this)->RankNeedRFM(ready_cmd.Rank(), true);
            const_cast<ChannelState*>(this)->ResetAlert();
            return Command();
        }
        
        // [RFM] Add to the RFM queue and return an empty command
        if (ready_cmd.cmd_type == CommandType::RFMsb)
        {
            // Calling a non-const function from a const function
            const_cast<ChannelState*>(this)->BanksetNeedRFM(ready_cmd.Rank(), ready_cmd.Bank(), true);
            return Command();    
        }

        if (ready_cmd.cmd_type == CommandType::RFMab)
        {
            // Calling a non-const function from a const function
            const_cast<ChannelState*>(this)->RankNeedRFM(ready_cmd.Rank(), true);
            return Command();    
        }

        if (ready_cmd.cmd_type == CommandType::ACTIVATE)
        {
            if (!ActivationWindowOk(ready_cmd.Rank(), clk))
            {
                return Command();
            }

            // [DRFM] this command will be activated unless something happens
            // So we run the selection logic before this command is executed

            BankState& bank_s = const_cast<ChannelState*>(this)->bank_states_[cmd.Rank()][cmd.Bankgroup()][cmd.Bank()];
            bool first = bank_s.IsSamplerFull();
            bool drfm_launch = false;

            if (config_.drfm_policy == 0) // Eager
            {
                drfm_launch = first;
            }
            
            if (!drfm_launch or config_.drfm_policy == 1)
            {

                // Check HYDRA RCC
                if (config_.hydra_mode == 1)
                {
                    int64_t rcc_state = bank_s.hydra_check_rcc(cmd);
                    bool is_inflight = true;
                    if (rcc_state == -1)
                    {
                        is_inflight = const_cast<ChannelState*>(this)->HydraRead(cmd.Rank(), cmd.Bankgroup(), cmd.Bank(), cmd.Row());
                    }
                    else if (rcc_state !=  0)
                    {
                        // std::cout << "HYDRA RCC: " << rcc_state << std::endl;
                        auto addr = config_.AddressMapping(rcc_state);
                        is_inflight = const_cast<ChannelState*>(this)->HydraRead(cmd.Rank(), cmd.Bankgroup(), cmd.Bank(), cmd.Row());
                        is_inflight &= const_cast<ChannelState*>(this)->HydraRead(addr.rank, addr.bankgroup, addr.bank, addr.row);
                        const_cast<ChannelState*>(this)->HydraWB(addr.rank, addr.bankgroup, addr.bank, addr.row);
                        // std::cout << "====================================" << std::endl;
                    }


                    // std::cout << "HYDRA RCC: " << rcc_state << " Inflight: " << is_inflight << std::endl;
                    // bank_s.PrintState();

                    if (!is_inflight)
                    {
                        return Command();
                    }
                }

                bool second = bank_s.PreACT(cmd);
                const_cast<ChannelState*>(this)->dream_preact(cmd.Rank(), cmd.Bankgroup(), cmd.Bank(), cmd.Row());
                const_cast<ChannelState*>(this)->abacus_preact(cmd.Rank(), cmd.Bankgroup(), cmd.Bank(), cmd.Row());

                if (config_.drfm_policy == 1) // Lazy, Lazy-Buffered
                {
                    drfm_launch = second;
                }
            }

            if (drfm_launch) // assuming timing constraints will always be met, because can't launch ACT without previous DRFM being done
            {
                bank_s.MarkDRFMIssued();
                switch(config_.drfm_mode)
                {
                    case 1:
                        const_cast<ChannelState*>(this)->BankNeedDRFM(cmd.Rank(), cmd.Bankgroup(), cmd.Bank(), true);
                        break;
                    case 2:
                        const_cast<ChannelState*>(this)->BanksetNeedDRFM(cmd.Rank(), cmd.Bank(), true);
                        break;
                    case 3:
                        const_cast<ChannelState*>(this)->RankNeedDRFM(cmd.Rank(), true);
                        break;
                    default:
                        AbruptExit(__FILE__, __LINE__);
                }
                return Command();
            }
        }
        return ready_cmd;
    }
}

void ChannelState::UpdateREFCounter(const Command& cmd)
{

    if (cmd.cmd_type == CommandType::REFsb)
    {
        fgr_counter_ = (fgr_counter_ + 1) % (2 * config_.banks_per_group);
        if (fgr_counter_ == 0)
        {
            ref_idx_ = (ref_idx_ + 1) % config_.refchunks;
        }
    }
    else if (cmd.cmd_type == CommandType::REFab)
    {
        fgr_counter_ = (fgr_counter_ + 1) % 2;
        if (config_.fgr and fgr_counter_ == 0)
        {
            ref_idx_ = (ref_idx_ + 1) % config_.refchunks;
        }
        else if (!config_.fgr)
        {
            ref_idx_ = (ref_idx_ + 1) % config_.refchunks;
        }
    }
    else
    {
        AbruptExit(__FILE__, __LINE__);
    }
}

int get_channel(const Config& config_, uint64_t hex_addr)
{
    hex_addr >>= config_.shift_bits;
    return (hex_addr >> config_.ch_pos) & config_.ch_mask;
}

void ChannelState::UpdateState(const Command& cmd, uint64_t clk)
{
    if (cmd.IsRankCMD()) {
        for (auto j = 0; j < config_.bankgroups; j++) {
            for (auto k = 0; k < config_.banks_per_group; k++) {
                bank_states_[cmd.Rank()][j][k].UpdateState(cmd, clk);
            }
        }
        if (cmd.IsRFM()) {
            RankNeedRFM(cmd.Rank(), false);
        } else if (cmd.IsRefresh()) {
            RankNeedRefresh(cmd.Rank(), false);
            UpdateREFCounter(cmd);
            dream_refresh();
            abacus_refresh();
        } else if (cmd.IsDRFM()) {
            RankNeedDRFM(cmd.Rank(), false);
            dream_mitig();
            abacus_mitig();
        } else if (cmd.cmd_type == CommandType::SREF_ENTER) {
            rank_is_sref_[cmd.Rank()] = true;
        } else if (cmd.cmd_type == CommandType::SREF_EXIT) {
            rank_is_sref_[cmd.Rank()] = false;
        }
    } 
    else if (cmd.IsSbCMD())
    {
        for (auto j = 0; j < config_.bankgroups; j++) {
            bank_states_[cmd.Rank()][j][cmd.Bank()].UpdateState(cmd, clk);
        }
        if (cmd.IsRFM()) {
            BanksetNeedRFM(cmd.Rank(), cmd.Bank(), false);
        } else if (cmd.IsDRFM()) {
            BanksetNeedDRFM(cmd.Rank(), cmd.Bank(), false);
        } else if (cmd.IsRefresh()) {
            BanksetNeedRefresh(cmd.Rank(), cmd.Bank(), false);
            UpdateREFCounter(cmd);
            dream_refresh();
            abacus_refresh();
        }
    }
    else
    {
        bank_states_[cmd.Rank()][cmd.Bankgroup()][cmd.Bank()].UpdateState(cmd, clk);
        if (cmd.IsRefresh()) {
            BankNeedRefresh(cmd.Rank(), cmd.Bankgroup(), cmd.Bank(), false);
        } else if (cmd.IsDRFM()) {
            BankNeedDRFM(cmd.Rank(), cmd.Bankgroup(), cmd.Bank(), false);
        }
    }

    if (cmd.IsReadWrite())
    {
        if (clk - last_bus_access_time_ == config_.burst_cycle)
        {
            bursty_access_count_++;
        }
        else
        {
            simple_stats_.AddValue("bursty_access_count", bursty_access_count_);
            bursty_access_count_ = 0;
        }
        last_bus_access_time_ = clk;
    }

    return;
}

void ChannelState::TriggerSameBankAlert(const Command& cmd, uint64_t clk)
{
    if (alert_n or num_acts_abo_ < config_.ABO_delay_acts)
    {
        return;
    }

    if (config_.alert_mode == 1 and bank_states_[cmd.Rank()][cmd.Bankgroup()][cmd.Bank()].CheckAlert())
    {
        alert_n = true;
        last_alert_clk_ = clk;
        simple_stats_.Increment("num_alerts");
    }
    return;
}


void ChannelState::TriggerSameRankAlert(const Command& cmd, uint64_t clk)
{
    if (alert_n or num_acts_abo_ < config_.ABO_delay_acts)
    {
        return;
    }

    for (auto j = 0; j < config_.bankgroups; j++) 
    {
        for (auto k = 0; k < config_.banks_per_group; k++)
        {
            if (alert_n)
            {
                return;
            }

            if (config_.alert_mode == 1 and bank_states_[cmd.Rank()][j][k].CheckAlert())
            {
                alert_n = true;
                last_alert_clk_ = clk;
                simple_stats_.Increment("num_alerts");
            }
        }
    }
    return;
}

void ChannelState::UpdateTiming(const Command& cmd, uint64_t clk)
{
    switch (cmd.cmd_type) {
        case CommandType::ACTIVATE:
            num_acts_abo_++;
            UpdateActivationTimes(cmd.Rank(), clk);
        case CommandType::READ_PRECHARGE:
        case CommandType::WRITE_PRECHARGE:
        case CommandType::PRECHARGE:
        case CommandType::PREab:
        case CommandType::PREsb:
            TriggerSameBankAlert(cmd, clk);
        case CommandType::READ:
        case CommandType::WRITE:
        case CommandType::REFRESH_BANK:
        case CommandType::DRFMb:
            // TODO - simulator speed? - Speciazlize which of the below
            // functions to call depending on the command type  Same Bank
            UpdateSameBankTiming(
                cmd.addr, timing_.same_bank[static_cast<int>(cmd.cmd_type)],
                clk);

            // Same Bankgroup other banks
            UpdateOtherBanksSameBankgroupTiming(
                cmd.addr,
                timing_
                    .other_banks_same_bankgroup[static_cast<int>(cmd.cmd_type)],
                clk);

            // Other bankgroups
            UpdateOtherBankgroupsSameRankTiming(
                cmd.addr,
                timing_
                    .other_bankgroups_same_rank[static_cast<int>(cmd.cmd_type)],
                clk);

            // Other ranks
            UpdateOtherRanksTiming(
                cmd.addr, timing_.other_ranks[static_cast<int>(cmd.cmd_type)],
                clk);
            break;

        case CommandType::RFMab: // [RFM] All Bank RFM
            num_acts_abo_ = 0;
        case CommandType::REFab:
            TriggerSameRankAlert(cmd, clk);
        case CommandType::DRFMab:
        case CommandType::SREF_ENTER:
        case CommandType::SREF_EXIT:
            UpdateSameRankTiming(
                cmd.addr, timing_.same_rank[static_cast<int>(cmd.cmd_type)],
                clk);
            break;
        case CommandType::REFsb: // [RFM] Same Bank RFM
        case CommandType::RFMsb: // [RFM] Same Bank RFM
        case CommandType::DRFMsb: // [DRFM] DRFM Same Bank
            TriggerSameRankAlert(cmd, clk);
            UpdateSameBankset(
                cmd.addr, timing_.same_bankset[static_cast<int>(cmd.cmd_type)],
                clk);
            
            UpdateOtherBanksets(
                cmd.addr, timing_.other_banksets[static_cast<int>(cmd.cmd_type)],
                clk);
            break;
        default:
            AbruptExit(__FILE__, __LINE__);
    }
    return;
}

void ChannelState::UpdateSameBankset(
    const Address& addr,
    const std::vector<std::pair<CommandType, int>>& cmd_timing_list,
    uint64_t clk) {
    for (auto j = 0; j < config_.bankgroups; j++) {
        for (auto cmd_timing : cmd_timing_list) {
            bank_states_[addr.rank][j][addr.bank].UpdateTiming(
                cmd_timing.first, clk + cmd_timing.second);
        }
    }
    return;
}

void ChannelState::UpdateOtherBanksets(
    const Address& addr,
    const std::vector<std::pair<CommandType, int>>& cmd_timing_list,
    uint64_t clk) {
    for (auto i = 0; i < config_.ranks; i++) {
        for (auto j = 0; j < config_.bankgroups; j++) {
            for (auto k = 0; k < config_.banks_per_group; k++) {
                if (k == addr.bank)
                {
                    continue;
                }
                for (auto cmd_timing : cmd_timing_list) {
                    bank_states_[i][j][k].UpdateTiming(
                        cmd_timing.first, clk + cmd_timing.second);
                }
            }
        }
    }
    return;
}

void ChannelState::UpdateSameBankTiming(
    const Address& addr,
    const std::vector<std::pair<CommandType, int>>& cmd_timing_list,
    uint64_t clk) {
    for (auto cmd_timing : cmd_timing_list) {
        bank_states_[addr.rank][addr.bankgroup][addr.bank].UpdateTiming(
            cmd_timing.first, clk + cmd_timing.second);
    }
    return;
}

void ChannelState::UpdateOtherBanksSameBankgroupTiming(
    const Address& addr,
    const std::vector<std::pair<CommandType, int>>& cmd_timing_list,
    uint64_t clk) {
    for (auto k = 0; k < config_.banks_per_group; k++) {
        if (k != addr.bank) {
            for (auto cmd_timing : cmd_timing_list) {
                bank_states_[addr.rank][addr.bankgroup][k].UpdateTiming(
                    cmd_timing.first, clk + cmd_timing.second);
            }
        }
    }
    return;
}

void ChannelState::UpdateOtherBankgroupsSameRankTiming(
    const Address& addr,
    const std::vector<std::pair<CommandType, int>>& cmd_timing_list,
    uint64_t clk) {
    for (auto j = 0; j < config_.bankgroups; j++) {
        if (j != addr.bankgroup) {
            for (auto k = 0; k < config_.banks_per_group; k++) {
                for (auto cmd_timing : cmd_timing_list) {
                    bank_states_[addr.rank][j][k].UpdateTiming(
                        cmd_timing.first, clk + cmd_timing.second);
                }
            }
        }
    }
    return;
}

void ChannelState::UpdateOtherRanksTiming(
    const Address& addr,
    const std::vector<std::pair<CommandType, int>>& cmd_timing_list,
    uint64_t clk) {
    for (auto i = 0; i < config_.ranks; i++) {
        if (i != addr.rank) {
            for (auto j = 0; j < config_.bankgroups; j++) {
                for (auto k = 0; k < config_.banks_per_group; k++) {
                    for (auto cmd_timing : cmd_timing_list) {
                        bank_states_[i][j][k].UpdateTiming(
                            cmd_timing.first, clk + cmd_timing.second);
                    }
                }
            }
        }
    }
    return;
}

void ChannelState::UpdateSameRankTiming(
    const Address& addr,
    const std::vector<std::pair<CommandType, int>>& cmd_timing_list,
    uint64_t clk) {
    for (auto j = 0; j < config_.bankgroups; j++) {
        for (auto k = 0; k < config_.banks_per_group; k++) {
            for (auto cmd_timing : cmd_timing_list) {
                bank_states_[addr.rank][j][k].UpdateTiming(
                    cmd_timing.first, clk + cmd_timing.second);
            }
        }
    }
    return;
}

void ChannelState::UpdateTimingAndStates(const Command& cmd, uint64_t clk) {

    UpdateState(cmd, clk);
    UpdateTiming(cmd, clk);

    // if (cmd.IsDRFM())
    // {
    //     std::cout <<  "===== CLK: " << clk << " =====" << std::endl; 
    //     std::cout << "CMD: " << cmd << std::endl;
    // }

    return;
}

bool ChannelState::ActivationWindowOk(int rank, uint64_t curr_time) const {
    bool tfaw_ok = IsFAWReady(rank, curr_time);
    if (config_.IsGDDR()) {
        if (!tfaw_ok)
            return false;
        else
            return Is32AWReady(rank, curr_time);
    }
    return tfaw_ok;
}

void ChannelState::UpdateActivationTimes(int rank, uint64_t curr_time) {
    if (!four_aw_[rank].empty() && curr_time >= four_aw_[rank][0]) {
        four_aw_[rank].erase(four_aw_[rank].begin());
    }
    four_aw_[rank].push_back(curr_time + config_.tFAW);
    if (config_.IsGDDR()) {
        if (!thirty_two_aw_[rank].empty() &&
            curr_time >= thirty_two_aw_[rank][0]) {
            thirty_two_aw_[rank].erase(thirty_two_aw_[rank].begin());
        }
        thirty_two_aw_[rank].push_back(curr_time + config_.t32AW);
    }
    return;
}

bool ChannelState::IsFAWReady(int rank, uint64_t curr_time) const {
    if (!four_aw_[rank].empty()) {
        if (curr_time < four_aw_[rank][0] && four_aw_[rank].size() >= 4) {
            return false;
        }
    }
    return true;
}

bool ChannelState::Is32AWReady(int rank, uint64_t curr_time) const {
    if (!thirty_two_aw_[rank].empty()) {
        if (curr_time < thirty_two_aw_[rank][0] &&
            thirty_two_aw_[rank].size() >= 32) {
            return false;
        }
    }
    return true;
}

}  // namespace dramsim3
