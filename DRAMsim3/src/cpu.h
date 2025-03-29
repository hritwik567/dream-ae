#ifndef __CPU_H
#define __CPU_H

#include <fstream>
#include <functional>
#include <random>
#include <string>
#include "memory_system.h"

namespace dramsim3 {

class CPU {
   public:
    CPU(const std::string& config_file, const std::string& output_dir)
        : memory_system_(
              config_file, output_dir,
              std::bind(&CPU::ReadCallBack, this, std::placeholders::_1),
              std::bind(&CPU::WriteCallBack, this, std::placeholders::_1)),
          clk_(0) {}
    virtual void ClockTick() = 0;
    virtual void ReadCallBack(uint64_t addr) { return; }
    virtual void WriteCallBack(uint64_t addr) { return; }
    void PrintStats() { memory_system_.PrintStats(false); }

   protected:
    MemorySystem memory_system_;
    uint64_t clk_;
};

class RandomCPU : public CPU {
   public:
    using CPU::CPU;
    void ClockTick() override;

   private:
    uint64_t last_addr_;
    bool last_write_ = false;
    std::mt19937_64 gen;
    bool get_next_ = true;
};

class StreamCPU : public CPU {
   public:
    using CPU::CPU;
    void ClockTick() override;

   private:
    uint64_t addr_a_, addr_b_, addr_c_, offset_ = 0;
    std::mt19937_64 gen;
    bool inserted_a_ = false;
    bool inserted_b_ = false;
    bool inserted_c_ = false;
    const uint64_t array_size_ = 2 << 20;  // elements in array
    const int stride_ = 64;                // stride in bytes
};

class TraceBasedCPU : public CPU {
   public:
    TraceBasedCPU(const std::string& config_file, const std::string& output_dir,
                  const std::string& trace_file);
    ~TraceBasedCPU() { trace_file_.close(); }
    void ClockTick() override;

   private:
    std::ifstream trace_file_;
    Transaction trans_;
    bool get_next_ = true;
};

class CovChCPU : public CPU {
   public:
    using CPU::CPU;
    void ClockTick() override;
    void ReadCallBack(uint64_t addr) override;
    void WriteCallBack(uint64_t addr) override;
    void Init();
    
    void TickSend();
    void TickRecv();

   private:
    std::mt19937_64 gen;
    Config* config_;

    // Tracking ref window
    uint64_t ref_window_;
    uint64_t ref_window_idx_;
    uint64_t prev_window_idx_ = -1;
    uint64_t warmup_idx_ = 2;




    // Sender
    bool send_init_ = false;
    std::array<uint64_t, 5> send_init_reqs_ = {70, 42, 48, 16, 16};

    uint64_t send_addr_;
    uint64_t send_row_idx_ = 0;
    uint64_t send_window_reqs_ = 0;
    uint64_t last_send_req_clk_ = 0;
    uint64_t send_limit_ = 0;
    uint64_t send_req_dist_ = 0;

    uint64_t send0_limit_ = 0;
    uint64_t send0_req_dist_ = 0;

    uint64_t send1_limit_ = 0;
    uint64_t send1_req_dist_ = 0;

    // Receiver
    uint64_t recv_addr_;
    uint64_t recv_row_idx_ = 0;          // Current window idx
    uint64_t recv_window_reqs_ = 0;      // Number of requests sent in the current window
    uint64_t last_recv_req_clk_ = 0;     // Clk when the last request was sent
    uint64_t recv_limit_ = 0;            // Num of ACTs to be done in a ref window
    uint64_t recv_req_dist_ = 0;         // Min distance between two requests

    // Timing constraints
    uint64_t spill_buffer_ = 300;   // Number of cycles reserved the spillovers from prev trefi window for recv
                                    // 300 should be able to handle 2 spills from the previous window
    uint64_t recv_footer_ = 150;    // Number of cycles at the end after which the receiver will not send any more requests
                                    // This is used by sender when sending its last request when sending 0
                                    // 150 should be able to handle the last request from the sender

    // Traversal 
    uint64_t offset_ = 512 * 1024; // 512 KB
    uint64_t max_rows_ = 100; // This should be enough for the DRAMSim3 to merge reads
};

}  // namespace dramsim3
#endif
