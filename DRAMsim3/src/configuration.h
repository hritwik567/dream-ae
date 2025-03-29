#ifndef __CONFIG_H
#define __CONFIG_H

#include <fstream>
#include <string>
#include "common.h"

#include "INIReader.h"
#include "lru.hpp"

namespace dramsim3 {

enum class DRAMProtocol {
    DDR3,
    DDR4,
    DDR5,
    GDDR5,
    GDDR5X,
    GDDR6,
    LPDDR,
    LPDDR3,
    LPDDR4,
    HBM,
    HBM2,
    HMC,
    SIZE
};

enum class RefreshPolicy {
    RANK_LEVEL_SIMULTANEOUS,  // impractical due to high power requirement
    RANK_LEVEL_STAGGERED,
    BANKSET_LEVEL_STAGGERED,
    BANK_LEVEL_STAGGERED,
    NO_REFRESH,
    SIZE 
};

enum class RowBufPolicy { 
    OPEN_PAGE,
    CLOSE_PAGE,
    SOFT_CLOSE_PAGE,
    SIZE
};

class Config {
   public:
    Config(std::string config_file, std::string out_dir);
    Address AddressMapping(uint64_t hex_addr) const;
    uint64_t ResetColBits(uint64_t hex_addr) const;
    uint64_t RemoveColBits(uint64_t hex_addr) const;
    // DRAM physical structure
    DRAMProtocol protocol;
    int channel_size;
    int channels;
    int ranks;
    int banks;
    int bankgroups;
    int banks_per_group;
    int rows;
    int columns;
    int device_width;
    int bus_width;
    int devices_per_rank;
    int BL;

    // Address mapping numbers
    int shift_bits;
    int ch_pos, ra_pos, bg_pos, ba_pos, ro_pos, co_pos, hi_pos, lo_pos;
    uint64_t ch_mask, ra_mask, bg_mask, ba_mask, ro_mask, co_mask, hi_mask, lo_mask;

    // Generic DRAM timing parameters
    double tCK;
    int burst_cycle;  // seperate BL with timing since for GDDRx it's not BL/2
    int AL;
    int CL;
    int CWL;
    int RL;
    int WL;
    int tCCD_L;
    int tCCD_S;
    int tRTRS;
    int tRTP;
    int tWTR_L;
    int tWTR_S;
    int tWR;
    int tRP;
    int tRRD_L;
    int tRRD_S;
    int tRAS;
    int tRCD;
    int tRC;
    // tCKSRE and tCKSRX are only useful for changing clock freq after entering
    // SRE mode we are not doing that, so tCKESR is sufficient
    int tCKE;
    int tCKESR;
    int tXS;
    int tXP;
    int tREFSBRD;
    int tRFC;
    int tRFCsb;
    int tRFCb;
    int tREFI;
    int tREFIsb;
    int tREFIb;
    int tFAW;
    int tRPRE;  // read preamble and write preamble are important
    int tWPRE;
    int read_delay;
    int write_delay;

    // LPDDR4 and GDDR5
    int tPPD;
    // GDDR5
    int t32AW;
    int tRCDRD;
    int tRCDWR;

    // pre calculated power parameters
    double act_energy_inc;
    double pre_energy_inc;
    double read_energy_inc;
    double write_energy_inc;
    double ref_energy_inc;
    double rfmab_energy_inc;
    double refb_energy_inc;
    double act_stb_energy_inc;
    double pre_stb_energy_inc;
    double pre_pd_energy_inc;
    double sref_energy_inc;

    // HMC
    int num_links;
    int num_dies;
    int link_width;
    int link_speed;
    int num_vaults;
    int block_size;  // block size in bytes
    int xbar_queue_depth;

    // System
    std::string address_mapping;
    int mop_size;
    bool mop_enabled = false;
    std::string queue_structure;
    RowBufPolicy row_buf_policy;
    RefreshPolicy refresh_policy;
    bool fgr = false;
    int refchunks;            // number of chunks to be refreshed for one time
    int rows_refreshed;       // number of rows to be refreshed for one time
    int cmd_queue_size;
    bool unified_queue;
    int trans_queue_size;
    int write_buf_size;
    bool enable_self_refresh;
    int sref_threshold;
    bool aggressive_precharging_enabled;
    bool enable_hbm_dual_cmd;


    // [RFM] parameters
    int rfm_mode; // 0: RFM disabled, 1: Same Bank RFM, 2: All Bank RFM, 
    int rfm_policy; // 0: RAAIMT (Eager), 1: RAAMMT (Lazy)
    int raaimt;
    int raammt;
    int rfm_raa_decrement;
    int ref_raa_decrement;
    int tRFM;
    int tRFMsb;

    // [ALERT] parameters
    int alert_mode;
    int tABO_act;             // the time for ABO to be activated
    int ABO_delay_acts;       // the number of ACTs before ABO is triggered
    int tABO_PW;              // the pulse width of ABO

    // [MOAT] parameters
    int moat_mode;
    int moatth;

    // [DRFM] parameters
    int drfm_mode; // 0: DRFM disabled, 1: DRFM Bank, 2: DRFM Same Bank, 3: DRFM All Bank
    int drfm_policy; // 0: Eager, 1: Lazy
    int drfm_qsize;
    int drfm_qth;
    int tDRFMb;
    int tDRFMsb;
    int tDRFMab;

    // [DREAM] parameters
    int dream_mode;
    int dream_policy;
    int dream_trhd;
    int dream_th;
    int dream_reset;
    int dream_k;
    bool dream_prev_enable;

    // [MINT] parameters
    int mint_mode;
    int mint_window;

    // [PARA] parameters
    int para_mode;
    double para_prob;

    // [Graphene] parameters
    int graphene_mode;
    int graphene_th; // trhd/2

    // [HYDRA] parameters
    int hydra_mode;
    int hydra_th;
    int hydra_gct_size;
    int hydra_gct_th;
    int hydra_rcc_sets;
    int hydra_rcc_ways;
    int hydra_wbq_size;
    LRUCache hydra_rcc;

    // [ABACUS] parameters
    int abacus_mode;
    int abacus_th;

    int epoch_period;
    int output_level;
    std::string output_dir;
    std::string output_prefix;
    std::string json_stats_name;
    std::string json_epoch_name;
    std::string txt_stats_name;

    // Computed parameters
    int request_size_bytes;

    bool IsGDDR() const {
        return (protocol == DRAMProtocol::GDDR5 ||
                protocol == DRAMProtocol::GDDR5X ||
                protocol == DRAMProtocol::GDDR6);
    }
    bool IsHBM() const {
        return (protocol == DRAMProtocol::HBM ||
                protocol == DRAMProtocol::HBM2);
    }
    bool IsHMC() const { return (protocol == DRAMProtocol::HMC); }
    // yzy: add another function
    bool IsDDR4() const { return (protocol == DRAMProtocol::DDR4); }

    bool IsDDR5() const { return (protocol == DRAMProtocol::DDR5); }

    int ideal_memory_latency;

#ifdef THERMAL
    std::string loc_mapping;
    int num_row_refresh;       // number of rows to be refreshed for one time
    double amb_temp;         // the ambient temperature in [C]
    double const_logic_power;

    double chip_dim_x;
    double chip_dim_y;
    int num_x_grids;
    int num_y_grids;
    int mat_dim_x;
    int mat_dim_y;
    // 0: x-direction priority, 1: y-direction priority
    int bank_order;
    // 0; low-layer priority, 1: high-layer priority
    int bank_layer_order;
    int row_tile;
    int tile_row_num;
    double bank_asr;  // the aspect ratio of a bank: #row_bits / #col_bits
#endif  // THERMAL

   private:
    INIReader* reader_;
    void CalculateSize();
    DRAMProtocol GetDRAMProtocol(std::string protocol_str);
    int GetInteger(const std::string& sec, const std::string& opt,
                   int default_val) const;
    void InitDRAMParams();
    void InitOtherParams();
    void InitPowerParams();
    void InitSystemParams();
    void InitRFMParams();
    void InitALERTParams();
    void InitDRFMParams();
    void InitDREAMParams();
    void InitMINTParams();
    void InitPARAParams();
    void InitMOATParams();
    void InitGrapheneParams();
    void InitHYDRAParams();
    void InitABACUSParams();
#ifdef THERMAL
    void InitThermalParams();
#endif  // THERMAL
    void InitTimingParams();
    void SetAddressMapping();
};

}  // namespace dramsim3
#endif
