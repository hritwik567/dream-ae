#ifndef __BANKSTATE_H
#define __BANKSTATE_H

#include <vector>
#include "common.h"
#include "configuration.h"
#include "simple_stats.h"

namespace dramsim3 {

struct DRFM_Q_Entry {
    uint32_t rowid;
    uint16_t ctr;

    DRFM_Q_Entry(uint32_t r, uint16_t c) : rowid(r), ctr(c) {}

    std::string to_string() const
    {
        return "RowID: " + std::to_string(rowid) + " Ctr: " + std::to_string(ctr);
    }
};

// [GRAPHENE]
struct GRAPHENE_Q_Entry {
    uint32_t rowid;
    uint16_t ctr;

    GRAPHENE_Q_Entry(uint32_t r, uint16_t c) : rowid(r), ctr(c) {}
};

class BankState {
   public:
    BankState(const Config& config, SimpleStats& simple_stats, int rank, int bank_group, int bank);

    enum class State { OPEN, CLOSED, SREF, PD, SIZE };
    Command GetReadyCommand(const Command& cmd, uint64_t clk) const;

    // Update the state of the bank resulting after the execution of the command
    void UpdateState(const Command& cmd, uint64_t clk);

    // Update the existing timing constraints for the command
    void UpdateTiming(const CommandType cmd_type, uint64_t time);

    bool IsRowOpen() const { return state_ == State::OPEN; }
    int OpenRow() const { return open_row_; }
    int RowHitCount() const { return row_hit_count_; }

    std::string StateToString(State state) const;
    void PrintState() const;

    // [REF]
    bool IsInREF() const;

    // [ALERT]
    bool CheckAlert();

    // [DRFM]
    bool PreACT(const Command& cmd);
    bool IsSamplerFull();
    void MarkDRFMIssued();
    bool IsInDRFM() const;

    void insert_drfm(uint32_t rowid);

    int64_t hydra_check_rcc(const Command& cmd);
   private:
    const Config& config_;
    SimpleStats& simple_stats_;

    // Current state of the Bank
    // Apriori or instantaneously transitions on a command.
    State state_;

    // Earliest time when the particular Command can be executed in this bank
    std::vector<uint64_t> cmd_timing_;

    Command last_cmd_;

    // Currently open row
    int open_row_;

    // consecutive accesses to one row
    int row_hit_count_;

    // rank, bank group, bank
    int rank_;
    int bank_group_;
    int bank_;

    // [RFM] RAA counter (Rolling Accumulated ACT)
    int raa_ctr_;

    // Activations
    std::string acts_stat_name_;
    std::string bank_identifier;
    int acts_counter_;

    // [REF]
    uint32_t ref_idx_;
    uint32_t fgr_counter_;

    // [PRAC] Counters
    std::vector<uint32_t> prac_;
    uint32_t max_prac_val_;

    // [DRFM]
    std::vector<DRFM_Q_Entry> drfm_q_;
    bool drfm_issued_;
    void drfm_post_act(uint32_t rowid);
    int64_t drfm_mitig();
    
    std::string mitig_used_stat_;
    std::string mitig_wasted_stat_;

    // [ABACUS]
    void abacus_mitig();

    // [DREAM]
    void dream_mitig();

    // [MOAT]
    void moat_act(uint32_t rowid);
    void moat_refresh();
    void moat_mitig();
    int moat_max_prac_idx_;

    // [MINT]
    void mint_preact(uint32_t rowid);
    void mint_refresh();
    void mint_mitig();
    std::vector<uint32_t> mint_rows_;

    // [PARA]
    void para_preact(uint32_t rowid);
    void para_refresh();
    void para_mitig();

    // [GRAPHENE]
    void graphene_preact(uint32_t rowid);
    void graphene_refresh();
    void graphene_mitig();
    std::vector<GRAPHENE_Q_Entry> graphene_q_;
    uint64_t graphene_spill_counter_;
    uint32_t graphene_entries_;
    std::string graphene_spills_stat_;
    std::string graphene_resets_stat_;

    // [Hydra]
    void hydra_preact(const Command& cmd);
    void hydra_refresh();
    void hydra_mitig();
    std::string hydra_resets_stat_;
    std::string hydra_gct_overflows_stat_;
    std::string hydra_aggressor_stat_;
    std::vector<uint32_t> hydra_gct_;
    std::vector<bool> hydra_gct_valid_;
    std::vector<uint32_t> hydra_counts_;
    uint32_t num_rows_per_gct_;

};

}  // namespace dramsim3
#endif
