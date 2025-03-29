#include "bankstate.h"

namespace dramsim3 {

BankState::BankState(const Config& config, SimpleStats& simple_stats, int rank, int bank_group, int bank)
    :   config_(config),
        simple_stats_(simple_stats),
        state_(State::CLOSED),
        cmd_timing_(static_cast<int>(CommandType::SIZE)),
        open_row_(-1),
        row_hit_count_(0),
        rank_(rank),
        bank_group_(bank_group),
        bank_(bank),
        raa_ctr_(0),
        acts_counter_(0),
        ref_idx_(0),
        fgr_counter_(0),
        prac_(config_.rows, 0),
        max_prac_val_(0)
{
    cmd_timing_[static_cast<int>(CommandType::READ)] = 0;
    cmd_timing_[static_cast<int>(CommandType::READ_PRECHARGE)] = 0;
    cmd_timing_[static_cast<int>(CommandType::WRITE)] = 0;
    cmd_timing_[static_cast<int>(CommandType::WRITE_PRECHARGE)] = 0;
    cmd_timing_[static_cast<int>(CommandType::ACTIVATE)] = 0;
    cmd_timing_[static_cast<int>(CommandType::PRECHARGE)] = 0;
    cmd_timing_[static_cast<int>(CommandType::REFRESH_BANK)] = 0;
    cmd_timing_[static_cast<int>(CommandType::REFsb)] = 0;
    cmd_timing_[static_cast<int>(CommandType::REFab)] = 0;
    cmd_timing_[static_cast<int>(CommandType::SREF_ENTER)] = 0;
    cmd_timing_[static_cast<int>(CommandType::SREF_EXIT)] = 0;
    cmd_timing_[static_cast<int>(CommandType::RFMsb)] = 0; // [RFM] Same Bank RFM
    cmd_timing_[static_cast<int>(CommandType::RFMab)] = 0; // [RFM] All Bank RFM
    cmd_timing_[static_cast<int>(CommandType::DRFMb)] = 0; // [DRFM] DRFM Bank
    cmd_timing_[static_cast<int>(CommandType::DRFMsb)] = 0; // [DRFM] DRFM Same Bank
    cmd_timing_[static_cast<int>(CommandType::DRFMab)] = 0; // [DRFM] DRFM All Bank
    cmd_timing_[static_cast<int>(CommandType::PREab)] = 0;
    cmd_timing_[static_cast<int>(CommandType::PREsb)] = 0;

    last_cmd_ = Command();

    // [Stats]
    bank_identifier = std::to_string(rank_) + "." + std::to_string(bank_group_) + "." + std::to_string(bank_);
    acts_stat_name_ = "acts." + bank_identifier;
    simple_stats_.InitStat(acts_stat_name_, "counter", "ACTs Counter");
    simple_stats_.InitHistoStat("acts_per_row_per_trefw", "ACTs per row per tREFW", 0, 65, 65);

    // [DRFM]
    drfm_issued_ = false;

    mitig_used_stat_ = "mitig_used." + bank_identifier;
    mitig_wasted_stat_ = "mitig_wasted." + bank_identifier;
    simple_stats_.InitStat(mitig_used_stat_, "counter", "Mitig Used Counter");
    simple_stats_.InitStat(mitig_wasted_stat_, "counter", "Mitig Wasted Counter");

    if (config_.dream_mode == 1)
    {
    }
    else if (config_.mint_mode == 1)
    {
    }
    else if (config_.para_mode == 1)
    {
    }
    else if (config_.graphene_mode == 1)
    {
        uint64_t max_acts = config_.refchunks * config_.tREFI / (config_.tRAS + config_.tRP);
        graphene_entries_ = max_acts / config_.graphene_th;
        graphene_q_.reserve(graphene_entries_);
        graphene_spill_counter_ = 0;

        graphene_spills_stat_ = "graphene_spills." + bank_identifier;
        simple_stats_.InitStat(graphene_spills_stat_, "counter", "Graphene Spills Counter");

        graphene_resets_stat_ = "graphene_resets." + bank_identifier;
        simple_stats_.InitStat(graphene_resets_stat_, "counter", "Graphene Resets Counter");
    }
    else if (config_.hydra_mode == 1)
    {
        num_rows_per_gct_ = config_.rows / config_.hydra_gct_size;
        hydra_gct_.resize(config_.hydra_gct_size, 0);
        hydra_gct_valid_.resize(config_.hydra_gct_size, true);
        hydra_counts_.resize(config_.rows, 0);

        hydra_resets_stat_ = "hydra_resets." + bank_identifier;
        simple_stats_.InitStat(hydra_resets_stat_, "counter", "Hydra Resets Counter");
    
        hydra_gct_overflows_stat_ = "hydra_gct_overflows." + bank_identifier;
        simple_stats_.InitStat(hydra_gct_overflows_stat_, "counter", "Hydra GCT Overflows Counter");

        hydra_aggressor_stat_ = "hydra_aggressor." + bank_identifier;
        simple_stats_.InitStat(hydra_aggressor_stat_, "counter", "Hydra Victim Counter");
    }
    else if (config_.moat_mode == 1)
    {
        moat_max_prac_idx_ = -1;
    }
}


Command BankState::GetReadyCommand(const Command& cmd, uint64_t clk) const {
    CommandType required_type = CommandType::SIZE;
    int rfm_th = config_.rfm_policy ? config_.raammt : config_.raaimt;

    switch (state_) {
        case State::CLOSED:
            switch (cmd.cmd_type) {
                // The state is closed and we need to activate the row to read/write
                case CommandType::READ:
                case CommandType::READ_PRECHARGE:
                case CommandType::WRITE:
                case CommandType::WRITE_PRECHARGE:
                    // [RFM] Block ACTs if RAA Counter == RAAMMT
                    // Don't block all banks, just the bank that wants to launch the command
                    if (config_.rfm_mode == 1 && raa_ctr_ >= rfm_th)
                    {
                        required_type = CommandType::RFMsb;
                    }
                    else if (config_.rfm_mode == 2 && raa_ctr_ >= rfm_th)
                    {
                        required_type = CommandType::RFMab;
                    }
                    else if (drfm_issued_)
                    {
                        required_type = CommandType::SIZE;
                    }
                    else
                    {
                        required_type = CommandType::ACTIVATE;
                    }
                    break;
                // The state is closed so we can issue and refresh commands
                case CommandType::REFRESH_BANK:
                case CommandType::REFsb:
                case CommandType::REFab:
                case CommandType::SREF_ENTER:
                case CommandType::RFMsb: // [RFM] Same Bank RFM
                case CommandType::RFMab: // [RFM] All Bank RFM
                case CommandType::DRFMb: // [DRFM] DRFM Bank
                case CommandType::DRFMsb: // [DRFM] DRFM Same Bank
                case CommandType::DRFMab: // [DRFM] DRFM All Bank
                    required_type = cmd.cmd_type;
                    break;
                default:
                    std::cerr << "Unknown type!" << std::endl;
                    AbruptExit(__FILE__, __LINE__);
                    break;
            }
            break;
        case State::OPEN:
            switch (cmd.cmd_type) {
                // The state is open and we get a RB hit if the row is the same else we PRECHARGE first
                case CommandType::READ:
                case CommandType::READ_PRECHARGE:
                case CommandType::WRITE:
                case CommandType::WRITE_PRECHARGE:
                    if (cmd.Row() == open_row_)
                    {
                        required_type = cmd.cmd_type;
                    }
                    else
                    {
                        required_type = CommandType::PRECHARGE;
                    }
                    break;
                // The state is open and a precharge command is issued to closed the row to perform refresh
                // TODO: Use PREsb and PREab here
                case CommandType::REFRESH_BANK:
                case CommandType::DRFMb: // [DRFM] DRFM Bank
                    required_type = CommandType::PRECHARGE;
                    break;
                case CommandType::REFab:  
                case CommandType::RFMab: // [RFM] All Bank RFM
                case CommandType::DRFMab: // [DRFM] DRFM All Bank
                case CommandType::SREF_ENTER:
                    required_type = CommandType::PREab;
                    break;
                case CommandType::RFMsb: // [RFM] Same Bank RFM
                case CommandType::REFsb:
                case CommandType::DRFMsb: // [DRFM] DRFM Same Bank
                    required_type = CommandType::PREsb;
                    break;
                default:
                    std::cerr << "Unknown type!" << std::endl;
                    AbruptExit(__FILE__, __LINE__);
                    break;
            }
            break;
        case State::SREF:
            switch (cmd.cmd_type) {
                // The state is SREF and to read/write we need to exit SREF using SREF_EXIT
                case CommandType::READ:
                case CommandType::READ_PRECHARGE:
                case CommandType::WRITE:
                case CommandType::WRITE_PRECHARGE:
                    required_type = CommandType::SREF_EXIT;
                    break;
                default:
                    std::cerr << "Unknown type!" << std::endl;
                    AbruptExit(__FILE__, __LINE__);
                    break;
            }
            break;
        case State::PD:
        case State::SIZE:
            std::cerr << "In unknown state" << std::endl;
            AbruptExit(__FILE__, __LINE__);
            break;
    }

    if (required_type != CommandType::SIZE)
    {
        if (clk >= cmd_timing_[static_cast<int>(required_type)])
        {
            return Command(required_type, cmd.addr, cmd.hex_addr);
        }

    }
    return Command();
}

bool BankState::IsInDRFM() const
{
    bool retval = last_cmd_.IsDRFM();
    return retval;
}

bool BankState::IsInREF() const
{
    bool retval = last_cmd_.IsRefresh();
    return retval;
}



void BankState::UpdateState(const Command& cmd, uint64_t clk) {

    if (cmd.cmd_type != CommandType::SIZE)
    {
        last_cmd_ = cmd;
    }

    switch (state_) {
        case State::OPEN:
            switch (cmd.cmd_type) {
                case CommandType::READ:
                case CommandType::WRITE:
                    row_hit_count_++;
                    break;
                // The state was open and a precharge command was issued which closed the row
                case CommandType::READ_PRECHARGE:
                case CommandType::WRITE_PRECHARGE:
                case CommandType::PRECHARGE:
                case CommandType::PREab:
                case CommandType::PREsb:
                    state_ = State::CLOSED;
                    open_row_ = -1;
                    row_hit_count_ = 0;
                    break;
                case CommandType::ACTIVATE:
                case CommandType::REFRESH_BANK:
                case CommandType::REFsb:
                case CommandType::REFab:
                case CommandType::SREF_ENTER:
                case CommandType::SREF_EXIT:
                case CommandType::RFMsb: // [RFM] Same Bank RFM
                case CommandType::RFMab: // [RFM] All Bank RFM
                case CommandType::DRFMb: // [DRFM] DRFM Bank
                case CommandType::DRFMsb: // [DRFM] DRFM Same Bank
                case CommandType::DRFMab: // [DRFM] DRFM All Bank
                default:
                    AbruptExit(__FILE__, __LINE__);
            }
            break;
        case State::CLOSED:
            switch (cmd.cmd_type) {
                case CommandType::REFsb: // FGR Mode
                case CommandType::REFRESH_BANK:
                case CommandType::REFab:
                    // [FGR]
                    fgr_counter_ = (fgr_counter_ + 1) % 2;

                    // [RFM]
                    raa_ctr_ -= std::min<int>(raa_ctr_, config_.ref_raa_decrement);

                    // When FGR is enabled, two REF commands are issued per tREFI
                    if ((config_.fgr and fgr_counter_ == 0) or !config_.fgr)
                    {
                        // [Stats]
                        acts_counter_ = 0;

                        // [MINT]
                        mint_refresh();

                        // [PARA]
                        para_refresh();

                        // [Graphene]
                        graphene_refresh();

                        // [HYDRA]
                        hydra_refresh();

                        // [MOAT]
                        moat_refresh();

                        // [PRAC]
                        for (int i = 0; i < config_.rows_refreshed; i++)
                        {
                            int idx = (ref_idx_ + i) % config_.rows;
                            simple_stats_.AddValue("acts_per_row_per_trefw", prac_[idx]);
                            max_prac_val_ = std::max<uint32_t>(max_prac_val_, prac_[idx]);
                            prac_[idx] = 0;
                        }

                        ref_idx_ = (ref_idx_ + config_.rows_refreshed) % config_.rows;
                    }
                    break;
                case CommandType::DRFMb: // [DRFM] DRFM Bank
                case CommandType::DRFMsb: // [DRFM] DRFM Same Bank
                case CommandType::DRFMab: // [DRFM] DRFM All Bank
                    // [DREAM]
                    dream_mitig();

                    // [MINT]
                    mint_mitig();

                    // [PARA]
                    para_mitig();

                    // [Graphene]
                    graphene_mitig();

                    // [HYDRA]
                    hydra_mitig();

                    // [ABACUS]
                    abacus_mitig();

                    drfm_issued_ = false;
                case CommandType::RFMab: // [RFM] All Bank RFM
                    // [MOAT]
                    moat_mitig();
                case CommandType::RFMsb: // [RFM] Same Bank RFM, cannot be used with PRAC+ABO
                    raa_ctr_ -= std::min<int>(raa_ctr_, config_.rfm_raa_decrement);
                    break;
                // The state was closed and an activate command was issued which opened the row
                case CommandType::ACTIVATE:
                    state_ = State::OPEN;
                    open_row_ = cmd.Row();

                    // [Stats]
                    acts_counter_++;
                    simple_stats_.Increment(acts_stat_name_);

                    // [RFM]
                    raa_ctr_++;

                    // [PRAC]                    
                    prac_[open_row_]++;

                    // [MOAT]
                    moat_act(open_row_);

                    // [DRFM]
                    drfm_post_act(open_row_);
                    break;
                // The state was closed and a refresh command was issued which changed the state to SREF
                case CommandType::SREF_ENTER:
                    state_ = State::SREF;
                    break;
                case CommandType::PREab:
                case CommandType::PREsb:
                    break;
                case CommandType::READ:
                case CommandType::WRITE:
                case CommandType::READ_PRECHARGE:
                case CommandType::WRITE_PRECHARGE:
                case CommandType::PRECHARGE:
                case CommandType::SREF_EXIT:
                default:
                    std::cout << cmd << std::endl;
                    AbruptExit(__FILE__, __LINE__);
            }
            break;
        case State::SREF:
            switch (cmd.cmd_type) {
                // The state was SREF and a SREF_EXIT command was issued which changed the state to closed
                case CommandType::SREF_EXIT:
                    state_ = State::CLOSED;
                    break;
                case CommandType::READ:
                case CommandType::WRITE:
                case CommandType::READ_PRECHARGE:
                case CommandType::WRITE_PRECHARGE:
                case CommandType::ACTIVATE:
                case CommandType::PRECHARGE:
                case CommandType::REFRESH_BANK:
                case CommandType::REFsb:
                case CommandType::REFab:
                case CommandType::SREF_ENTER:
                case CommandType::RFMsb: // [RFM] Same Bank RFM
                case CommandType::RFMab: // [RFM] All Bank RFM
                case CommandType::DRFMb: // [DRFM] DRFM Bank
                case CommandType::DRFMsb: // [DRFM] DRFM Same Bank
                case CommandType::DRFMab: // [DRFM] DRFM All Bank
                case CommandType::PREab:
                case CommandType::PREsb:
                default:
                    AbruptExit(__FILE__, __LINE__);
            }
            break;
        default:
            AbruptExit(__FILE__, __LINE__);
    }
    return;
}

void BankState::UpdateTiming(CommandType cmd_type, uint64_t time) {
    cmd_timing_[static_cast<int>(cmd_type)] =
        std::max(cmd_timing_[static_cast<int>(cmd_type)], time);
    return;
}

std::string BankState::StateToString(State state) const {
    switch (state) {
        case State::OPEN:
            return "OPEN";
        case State::CLOSED:
            return "CLOSED";
        case State::SREF:
            return "SREF";
        case State::PD:
            return "PD";
        case State::SIZE:
            return "SIZE";
    }
    return "UNKNOWN";
}

void BankState::PrintState() const {
    std::cout << "Bank: " << bank_identifier << std::endl;
    std::cout << "State: " << StateToString(state_) << std::endl;
    std::cout << "Open Row: " << open_row_ << std::endl;
}

bool BankState::CheckAlert()
{
    if (config_.moat_mode == 1)
    {
        return (moat_max_prac_idx_ != -1 and prac_[moat_max_prac_idx_] > config_.moatth);
    }
    return false;
}

bool BankState::IsSamplerFull()
{
    if (config_.drfm_mode == 0) return false;

    // std::cout << bank_identifier << ".IsSamplerFull: " << drfm_q_.size() << " " << config_.drfm_qsize << std::endl;
    if (drfm_q_.size() >= (uint32_t)config_.drfm_qsize)
    {
        return true;
    }

    auto max_entry = std::max_element(drfm_q_.begin(), drfm_q_.end(), [] (const DRFM_Q_Entry& a, const DRFM_Q_Entry& b) {
        return a.ctr < b.ctr;
    });

    if (max_entry != drfm_q_.end())
    {
        return max_entry->ctr >= config_.drfm_qth;
    }

    return false;
}

void BankState::MarkDRFMIssued()
{
    drfm_issued_ = true;
}

bool BankState::PreACT(const Command& cmd)
{
    uint32_t rowid = cmd.Row();

    // [MINT]
    mint_preact(rowid);

    // [PARA]
    para_preact(rowid);

    // [Graphene]
    graphene_preact(rowid);

    // [HYDRA]
    hydra_preact(cmd);

    return IsSamplerFull();
}

void BankState::drfm_post_act(uint32_t rowid)
{
    auto found = std::find_if(drfm_q_.begin(), drfm_q_.end(), [rowid](const DRFM_Q_Entry& entry) {
        return entry.rowid == rowid;
    });

    if (found != drfm_q_.end())
    {
        found->ctr++;
    }
}

void BankState::insert_drfm(uint32_t rowid)
{
    if (config_.drfm_mode == 0) return;

    drfm_q_.emplace_back(rowid, 0);
}

void BankState::mint_preact(uint32_t rowid)
{
    if (config_.mint_mode == 0) return;

    mint_rows_.push_back(rowid);

    if (mint_rows_.size() >= (uint32_t)config_.mint_window)
    {
        // Pick a random row from mint_rows_
        uint32_t idx = rand() % mint_rows_.size();
        uint32_t select_rowid = mint_rows_[idx];

        mint_rows_.clear();

        assert(drfm_q_.size() <= config_.drfm_qsize);
        drfm_q_.emplace_back(select_rowid, 0);
    }
}

void BankState::para_preact(uint32_t rowid)
{
    if (config_.para_mode == 0) return;

    double targ_prob = config_.para_prob;
    double randval = static_cast<double>(rand() % (1 << 20)) / static_cast<double>(1 << 20);
    if (randval < targ_prob)
    {
        assert(drfm_q_.size() <= config_.drfm_qsize);
        drfm_q_.emplace_back(rowid, 0);
    }

}

void BankState::graphene_preact(uint32_t rowid)
{
    if (config_.graphene_mode == 0) return;

    auto found = std::find_if(graphene_q_.begin(), graphene_q_.end(), [rowid](const GRAPHENE_Q_Entry& entry) {
        return entry.rowid == rowid;
    });

    if (found != graphene_q_.end())
    {
        found->ctr++;
    }
    else
    {
        if (graphene_q_.size() < graphene_entries_)
        {
            graphene_q_.emplace_back(rowid, 1);
        }
        else
        {
            auto low_count_entry = std::find_if(graphene_q_.begin(), graphene_q_.end(), [this] (const GRAPHENE_Q_Entry& entry) {
                return entry.ctr == graphene_spill_counter_;
            });

            if (low_count_entry != graphene_q_.end())
            {
                low_count_entry->rowid = rowid;
                low_count_entry->ctr = graphene_spill_counter_ + 1;
            }
            else
            {
                graphene_spill_counter_++;
                simple_stats_.Increment(graphene_spills_stat_);
            }
        }
    }

    // max element in graphene_q_ is greater than or equal to graphene_th
    auto found_tg = std::find_if(graphene_q_.begin(), graphene_q_.end(), [this] (const GRAPHENE_Q_Entry& entry) {
        return entry.ctr >= config_.graphene_th;
    });

    if (found_tg != graphene_q_.end())
    {
        assert(drfm_q_.size() <= config_.drfm_qsize);
        drfm_q_.emplace_back(found_tg->rowid, 0);
    }
}

int64_t BankState::hydra_check_rcc(const Command& cmd)
{
    if (config_.hydra_mode == 0) return 0;
    
    uint32_t rowid = cmd.Row();
    uint32_t gct_idx = rowid % config_.hydra_gct_size;

    if (hydra_gct_valid_[gct_idx])
    {
        return 0;
    }

    uint64_t rcc_tag = config_.ResetColBits(cmd.hex_addr);
    uint64_t rcc_set = config_.RemoveColBits(cmd.hex_addr);
    return const_cast<LRUCache*>(&config_.hydra_rcc)->read(rcc_tag, rcc_set);
}

void BankState::hydra_preact(const Command& cmd)
{
    if (config_.hydra_mode == 0) return;
    
    uint32_t rowid = cmd.Row();
    uint32_t gct_idx = rowid % config_.hydra_gct_size;

    if (hydra_gct_valid_[gct_idx])
    {
        hydra_gct_[gct_idx]++;
    }
    else
    {
        hydra_counts_[rowid]++;
        uint64_t rcc_tag = config_.ResetColBits(cmd.hex_addr);
        uint64_t rcc_set = config_.RemoveColBits(cmd.hex_addr);
        const_cast<LRUCache*>(&config_.hydra_rcc)->write(rcc_tag, rcc_set);
    }

    if (hydra_gct_[gct_idx] >= config_.hydra_gct_th and hydra_gct_valid_[gct_idx])
    {
        simple_stats_.Increment(hydra_gct_overflows_stat_);
        hydra_gct_valid_[gct_idx] = false;
        for (int i = 0; i < num_rows_per_gct_; i++)
        {
            hydra_counts_[i * config_.hydra_gct_size  + gct_idx] = config_.hydra_gct_th;
        }
    }

    if (hydra_gct_valid_[gct_idx])
    {
        return;
    }

    if (hydra_counts_[rowid] >= config_.hydra_th)
    {
        simple_stats_.Increment(hydra_aggressor_stat_);
        assert(drfm_q_.size() <= config_.drfm_qsize);
        drfm_q_.emplace_back(rowid, 0);
    }
}

void BankState::mint_refresh()
{
    if (config_.mint_mode == 0) return;
}

void BankState::para_refresh()
{
    if (config_.para_mode == 0) return;
}

void BankState::graphene_refresh()
{
    if (config_.graphene_mode == 0) return;

    if (ref_idx_ % config_.rows == 0)
    {
        graphene_q_.clear();
        graphene_spill_counter_ = 0;
        simple_stats_.Increment(graphene_resets_stat_);
    }
}

void BankState::hydra_refresh()
{
    if (config_.hydra_mode == 0) return;

    if (ref_idx_ % config_.rows == 0)
    {
        hydra_gct_.clear();
        hydra_gct_.resize(config_.hydra_gct_size, 0);

        hydra_gct_valid_.clear();
        hydra_gct_valid_.resize(config_.hydra_gct_size, true);

        hydra_counts_.clear();
        hydra_counts_.resize(config_.rows, 0);

        simple_stats_.Increment(hydra_resets_stat_);
    }
}

void BankState::dream_mitig()
{
    if (config_.dream_mode == 0) return;

    drfm_mitig();
}


void BankState::mint_mitig()
{
    if (config_.mint_mode == 0) return;

    drfm_mitig();
}

void BankState::para_mitig()
{
    if (config_.para_mode == 0) return;

    drfm_mitig();
}

void BankState::graphene_mitig()
{
    if (config_.graphene_mode == 0) return;

    int64_t rowid = drfm_mitig();

    auto max_entry = std::max_element(graphene_q_.begin(), graphene_q_.end(), [] (const GRAPHENE_Q_Entry& a, const GRAPHENE_Q_Entry& b) {
        return a.ctr < b.ctr;
    });

    if (max_entry != graphene_q_.end())
    {
        assert(rowid != -1 and rowid == max_entry->rowid);
        graphene_q_.erase(max_entry);
    }
}

void BankState::hydra_mitig()
{
    if (config_.hydra_mode == 0) return;

    int64_t rowid = drfm_mitig();

    if (rowid != -1)
    {
        hydra_counts_[rowid] = 0;
    }
}

void BankState::abacus_mitig()
{
    if (config_.abacus_mode == 0) return;

    drfm_mitig();
}

int64_t BankState::drfm_mitig()
{
    // Find max drfm q entry, if all the values same give me the first one
    auto max_entry = std::max_element(drfm_q_.begin(), drfm_q_.end(), [] (const DRFM_Q_Entry& a, const DRFM_Q_Entry& b) {
        return a.ctr < b.ctr;
    });

    if (max_entry != drfm_q_.end()) 
    {
        if (max_entry->ctr <= drfm_q_[0].ctr)
            max_entry = drfm_q_.begin();
    
        simple_stats_.Increment(mitig_used_stat_);
        drfm_q_.erase(max_entry);
        return max_entry->rowid;
    }
    else
    {
        simple_stats_.Increment(mitig_wasted_stat_);
    }

    return -1;
}


// [MOAT]
void BankState::moat_act(uint32_t rowid)
{
    if (config_.moat_mode == 0) return;

    if (moat_max_prac_idx_ == -1)
    {
        moat_max_prac_idx_ = rowid;
    }
    else if (prac_[rowid] > prac_[moat_max_prac_idx_])
    {
        moat_max_prac_idx_ = rowid;
    }
}

void BankState::moat_refresh()
{
    if (config_.moat_mode == 0) return;

    // If the moat_max_prac_idx_ or moat_cur_mitig_idx_ is refresed, reset them
    if ((moat_max_prac_idx_ >= ref_idx_) and (moat_max_prac_idx_ < (ref_idx_ + config_.rows_refreshed)))
    {
        moat_max_prac_idx_ = -1;
    }
}

void BankState::moat_mitig()
{
    if (config_.moat_mode == 0) return;

    if (moat_max_prac_idx_ == -1)
    {
        return;
    }

    prac_[moat_max_prac_idx_] = 0;
    
    if (moat_max_prac_idx_ > 0) prac_[moat_max_prac_idx_ - 1]++;
    if (moat_max_prac_idx_ > 1) prac_[moat_max_prac_idx_ - 2]++;
    if (moat_max_prac_idx_ < config_.rows - 1) prac_[moat_max_prac_idx_ + 1]++;
    if (moat_max_prac_idx_ < config_.rows - 2) prac_[moat_max_prac_idx_ + 2]++;

    moat_max_prac_idx_ = -1;
}


}  // namespace dramsim3
