#ifndef __CHANNEL_STATE_H
#define __CHANNEL_STATE_H

#include <vector>
#include <string>
#include <sstream>
#include "bankstate.h"
#include "common.h"
#include "configuration.h"
#include "timing.h"
#include "simple_stats.h"


namespace dramsim3 {

// [ABACUS]
struct ABACUS_Entry {
    uint32_t rac;
    uint64_t sav;

    ABACUS_Entry(uint32_t a, uint64_t s) : rac(a), sav(s) {}
    ABACUS_Entry() : rac(0), sav(0) {}


    std::string to_string() const
    {
        std::stringstream ss;
        ss << " RAC: " << rac << " SAV: " << std::hex << sav << std::dec;
        return ss.str();
    }
};

class ChannelState {
   public:
    ChannelState(const Config& config, const Timing& timing, SimpleStats& simple_stats, int channel);
    Command GetReadyCommand(const Command& cmd, uint64_t clk) const;
    void UpdateState(const Command& cmd, uint64_t clk);
    void UpdateTiming(const Command& cmd, uint64_t clk);
    void UpdateTimingAndStates(const Command& cmd, uint64_t clk);
    bool ActivationWindowOk(int rank, uint64_t curr_time) const;
    void UpdateActivationTimes(int rank, uint64_t curr_time);
    bool IsRowOpen(int rank, int bankgroup, int bank) const {
        return bank_states_[rank][bankgroup][bank].IsRowOpen();
    }
    bool IsAllBankIdleInRank(int rank) const;
    bool IsRankSelfRefreshing(int rank) const { return rank_is_sref_[rank]; }
    bool IsRefreshWaiting() const { return !refresh_q_.empty(); }
    bool IsRWPendingOnRef(const Command& cmd) const;
    const Command& PendingRefCommand() const {return refresh_q_.front(); }
    void BankNeedRefresh(int rank, int bankgroup, int bank, bool need);
    void BanksetNeedRefresh(int rank, int bank, bool need);
    void RankNeedRefresh(int rank, bool need);
    
    // [DRFM] and [RFM]
    void BankNeedDRFM(int rank, int bankgroup, int bank, bool need);
    void RankNeedDRFM(int rank, bool need);
    void BanksetNeedDRFM(int rank, int bank, bool need);

    void RankNeedRFM(int rank, bool need);
    void BanksetNeedRFM(int rank, int bank, bool need);
    bool IsRFMWaiting() const { return !rfm_q_.empty(); }
    const Command& PendingRFMCommand() const {return rfm_q_.front(); }

    // [Hydra]
    bool HydraRead(int rank, int bankgroup, int bank, int row);
    void HydraWB(int rank, int bankgroup, int bank, int row);
    Command GetReadyHydraCommand(uint64_t clk) const;
    bool hydra_wb_draining_;
    
    int OpenRow(int rank, int bankgroup, int bank) const {
        return bank_states_[rank][bankgroup][bank].OpenRow();
    }
    int RowHitCount(int rank, int bankgroup, int bank) const {
        return bank_states_[rank][bankgroup][bank].RowHitCount();
    };

    void PrintDeadlock() const;
    bool IsInDRFM(int rank, int bankgroup, int bank) const {
        return bank_states_[rank][bankgroup][bank].IsInDRFM();
    }

    bool IsInREF(int rank, int bankgroup, int bank) const {
        return bank_states_[rank][bankgroup][bank].IsInREF();
    }

    std::vector<int> rank_idle_cycles;

   private:
    const Config& config_;
    const Timing& timing_;
    SimpleStats& simple_stats_;
    int channel_;

    // [Tracking]
    uint64_t last_bus_access_time_;
    uint64_t bursty_access_count_;

    std::vector<bool> rank_is_sref_;
    std::vector<std::vector<std::vector<BankState> > > bank_states_;
    std::vector<Command> refresh_q_;
    std::vector<Command> rfm_q_; // [RFM]
    std::vector<Command> hydra_rd_q_; // [Hydra]
    std::vector<Command> hydra_wb_q_; // [Hydra]

    // [REF]
    uint32_t ref_idx_;
    uint32_t fgr_counter_;
    void UpdateREFCounter(Command& cmd);

    std::vector<std::vector<uint64_t> > four_aw_;
    std::vector<std::vector<uint64_t> > thirty_two_aw_;
    bool IsFAWReady(int rank, uint64_t curr_time) const;
    bool Is32AWReady(int rank, uint64_t curr_time) const;
    
    // [DREAM]
    std::string dream_resets_stat_;
    std::vector<uint32_t> tusc_; // Table of Untagged Skewed Counters
    std::vector<uint32_t> tusc_prev_; // Table of Untagged Skewed Counters
    std::vector<uint32_t> random_masks;
    uint32_t tusc_size_;
    std::vector<uint32_t> tusc_q_; // Queue of tusc_idx
    uint32_t get_tusc_idx(uint32_t rank, uint32_t bankgroup, uint32_t bank, uint32_t rowid) const;
    uint32_t get_row_idx(uint32_t rank, uint32_t bankgroup, uint32_t bank, uint32_t tuscid, uint32_t row_num) const;
    void dream_preact(uint32_t rank, uint32_t bankgroup, uint32_t bank, uint32_t rowid);
    void dream_refresh();
    void dream_mitig();

    // [ABACUS]
    std::vector<ABACUS_Entry> abacus_table_;
    std::vector<uint32_t> abacus_q_;
    uint32_t abacus_entries_;
    std::string abacus_resets_stat_;
    void abacus_preact(uint32_t rank, uint32_t bankgroup, uint32_t bank, uint32_t rowid);
    void abacus_refresh();
    void abacus_mitig();

    // [ABO]
    bool alert_n;
    uint64_t last_alert_clk_;
    int num_acts_abo_;
    void TriggerSameBankAlert(const Command& cmd, uint64_t clk);
    void TriggerSameRankAlert(const Command& cmd, uint64_t clk);
    void ResetAlert() { alert_n = false; }

    void UpdateREFCounter(const Command& cmd);

    void UpdateSameBankset(
        const Address& addr,
        const std::vector<std::pair<CommandType, int> >& cmd_timing_list,
        uint64_t clk);
    
    void UpdateOtherBanksets(
        const Address& addr,
        const std::vector<std::pair<CommandType, int> >& cmd_timing_list,
        uint64_t clk);

    // Update timing of the bank the command corresponds to    
    void UpdateSameBankTiming(
        const Address& addr,
        const std::vector<std::pair<CommandType, int> >& cmd_timing_list,
        uint64_t clk);

    // Update timing of the other banks in the same bankgroup as the command
    void UpdateOtherBanksSameBankgroupTiming(
        const Address& addr,
        const std::vector<std::pair<CommandType, int> >& cmd_timing_list,
        uint64_t clk);

    // Update timing of banks in the same rank but different bankgroup as the
    // command
    void UpdateOtherBankgroupsSameRankTiming(
        const Address& addr,
        const std::vector<std::pair<CommandType, int> >& cmd_timing_list,
        uint64_t clk);

    // Update timing of banks in a different rank as the command
    void UpdateOtherRanksTiming(
        const Address& addr,
        const std::vector<std::pair<CommandType, int> >& cmd_timing_list,
        uint64_t clk);

    // Update timing of the entire rank (for rank level commands)
    void UpdateSameRankTiming(
        const Address& addr,
        const std::vector<std::pair<CommandType, int> >& cmd_timing_list,
        uint64_t clk);
};

}  // namespace dramsim3
#endif
