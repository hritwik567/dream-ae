#include "cpu.h"

namespace dramsim3 {

void RandomCPU::ClockTick() {
    // Create random CPU requests at full speed
    // this is useful to exploit the parallelism of a DRAM protocol
    // and is also immune to address mapping and scheduling policies
    memory_system_.ClockTick();
    if (get_next_) {
        last_addr_ = gen();
        last_write_ = (gen() % 3 == 0);
    }
    get_next_ = memory_system_.WillAcceptTransaction(last_addr_, last_write_);
    if (get_next_) {
        memory_system_.AddTransaction(last_addr_, last_write_);
    }
    clk_++;
    return;
}

void StreamCPU::ClockTick() {
    // stream-add, read 2 arrays, add them up to the third array
    // this is a very simple approximate but should be able to produce
    // enough buffer hits

    // moving on to next set of arrays
    memory_system_.ClockTick();
    if (offset_ >= array_size_ || clk_ == 0) {
        addr_a_ = gen();
        addr_b_ = gen();
        addr_c_ = gen();
        offset_ = 0;
    }

    if (!inserted_a_ &&
        memory_system_.WillAcceptTransaction(addr_a_ + offset_, false)) {
        memory_system_.AddTransaction(addr_a_ + offset_, false);
        inserted_a_ = true;
    }
    if (!inserted_b_ &&
        memory_system_.WillAcceptTransaction(addr_b_ + offset_, false)) {
        memory_system_.AddTransaction(addr_b_ + offset_, false);
        inserted_b_ = true;
    }
    if (!inserted_c_ &&
        memory_system_.WillAcceptTransaction(addr_c_ + offset_, true)) {
        memory_system_.AddTransaction(addr_c_ + offset_, true);
        inserted_c_ = true;
    }
    // moving on to next element
    if (inserted_a_ && inserted_b_ && inserted_c_) {
        offset_ += stride_;
        inserted_a_ = false;
        inserted_b_ = false;
        inserted_c_ = false;
    }
    clk_++;
    return;
}

TraceBasedCPU::TraceBasedCPU(const std::string& config_file,
                             const std::string& output_dir,
                             const std::string& trace_file)
    : CPU(config_file, output_dir) {
    trace_file_.open(trace_file);
    if (trace_file_.fail()) {
        std::cerr << "Trace file does not exist" << std::endl;
        AbruptExit(__FILE__, __LINE__);
    }
}

void TraceBasedCPU::ClockTick() {
    memory_system_.ClockTick();
    if (!trace_file_.eof()) {
        if (get_next_) {
            get_next_ = false;
            trace_file_ >> trans_;
        }
        if (trans_.added_cycle <= clk_) {
            get_next_ = memory_system_.WillAcceptTransaction(trans_.addr,
                                                             trans_.is_write);
            if (get_next_) {
                memory_system_.AddTransaction(trans_.addr, trans_.is_write);
            }
        }
    }
    clk_++;
    return;
}

void CovChCPU::Init()
{
    config_ = memory_system_.GetConfig();
    uint64_t ch_mask = (0x1 << 13) ^ 0xFFFFFFFFFFFFFFFFull;
    uint64_t size_mask = (1ull << 35) - 1;
    uint64_t base_addr = gen() & ch_mask & size_mask;
    // Recv Init
    
    uint64_t bank0_mask = (0x1F << 14) ^ 0xFFFFFFFFFFFFFFFFull;
    recv_addr_ = base_addr & bank0_mask;
    Address recv_addr_obj_ = config_->AddressMapping(recv_addr_);

    recv_limit_ = config_->ref_raa_decrement;
    recv_req_dist_ = (config_->tREFI - config_->tRFC - spill_buffer_ - recv_footer_) / recv_limit_;
    
    std::cout << "[Recv] [Addr = " << recv_addr_ << "]" << std::endl;
    std::cout << "[Recv] [channel = " << recv_addr_obj_.channel << "] [rank = " << recv_addr_obj_.rank << "] [bankgroup = " << recv_addr_obj_.bankgroup << "] [bank = " << recv_addr_obj_.bank << "] [row = " << recv_addr_obj_.row << "] [column = " << recv_addr_obj_.column << "]" << std::endl;
    std::cout << "[Recv] [recv_req_dist_ = " << recv_req_dist_  << "] [recv_limit_ = " << recv_limit_ << "]" << std::endl;

    // Send Init
    send_init_ = false;

    uint64_t bank2_mask = (0x0F << 14) ^ 0xFFFFFFFFFFFFFFFFull;
    send_addr_ = base_addr & bank2_mask;
    Address send_addr_obj_ = config_->AddressMapping(send_addr_);

    send0_limit_ = config_->ref_raa_decrement;
    send0_req_dist_ = (config_->tREFI - config_->tRFC - spill_buffer_) / send0_limit_;

    send1_limit_ = config_->rfm_raa_decrement + config_->ref_raa_decrement;
    send1_req_dist_ = (config_->tREFI - config_->tRFC - spill_buffer_ - recv_footer_ - (recv_req_dist_ * 2)) / send1_limit_;

    std::cout << "[Send] [Bank = 2] [Addr = " << std::hex << send_addr_ << std::dec << "]" << std::endl;
    std::cout << "[Send] [channel = " << send_addr_obj_.channel << "] [rank = " << send_addr_obj_.rank << "] [bankgroup = " << send_addr_obj_.bankgroup << "] [bank = " << send_addr_obj_.bank << "] [row = " << send_addr_obj_.row << "] [column = " << send_addr_obj_.column << "]" << std::endl;
    std::cout << "[Send] [send0_req_dist_ = " << send0_req_dist_  << "] [send0_limit_ = " << send0_limit_ << "]" << std::endl;
    std::cout << "[Send] [send1_req_dist_ = " << send1_req_dist_  << "] [send1_limit_ = " << send1_limit_ << "]" << std::endl; 

}

void CovChCPU::ReadCallBack(uint64_t addr)
{
    // std::cout << "ReadCallBack: " << std::hex << addr << std::dec << std::endl;
}

void CovChCPU::WriteCallBack(uint64_t addr)
{
    // std::cout << "WriteCallBack: " << std::hex << addr << std::dec << std::endl;
}

void CovChCPU::TickSend()
{
    // Update the reqs counter in the begining of a window
    if (prev_window_idx_ != ref_window_idx_)
    {
        send_window_reqs_ = 0;
    }

    if (!send_init_)
    {
        if (ref_window_idx_ - warmup_idx_ >= send_init_reqs_.size())
        {
            send_init_ = true;
        }
        else if (send_window_reqs_ >= send_init_reqs_[ref_window_idx_ - warmup_idx_])
        {
            return;
        }
    }

    if (send_init_)
    {
        if (prev_window_idx_ != ref_window_idx_)
        {

            uint64_t rand = gen() % 2;
            // uint64_t rand = ref_window_idx_ % 2;
            std::cout << " rand = " << rand << " ref_window_idx_ = " << ref_window_idx_ << std::endl;
            if (rand == 0)
            {
                send_limit_ = send0_limit_;
                send_req_dist_ = send0_req_dist_;
            }
            else
            {
                send_limit_ = send1_limit_;
                send_req_dist_ = send1_req_dist_;
            }
        }

        if (send_window_reqs_ >= send_limit_)
        {
            return;
        }

        // Wait for the spill buffer to clear
        if (ref_window_ < (spill_buffer_ + config_->tRFC))
        {
            return;
        }

        // Wait for the appropriate time to send
        if (clk_ - last_send_req_clk_ < send_req_dist_)
        {
            return;
        }

    }


    uint64_t addr_ = send_addr_ + send_row_idx_ * offset_;
    if (memory_system_.WillAcceptTransaction(addr_, false))
    {
    // std::cout << "[" << send_init_  << "] [Send] window_idx = " << ref_window_idx_ << " last_send_window_idx_ = " << last_send_window_idx_ << " send_window_reqs_ = " << send_window_reqs_ << std::endl;
    // std::cout << "[Send] clk_ = " << clk_ << " last_send_req_clk_ = " << last_send_req_clk_ << " send_req_dist_ = " << send_req_dist_ << std::endl;
        memory_system_.AddTransaction(addr_, false);
        send_row_idx_ = (send_row_idx_ + 1) % max_rows_;
        send_window_reqs_++;
        last_send_req_clk_ = clk_;
    }
}

void CovChCPU::TickRecv()
{
    // Update the reqs counter in the begining of a window
    if (prev_window_idx_ != ref_window_idx_)
    {
        recv_window_reqs_ = 0;
    }

    // Wait for the sender to init
    if (!send_init_)
    {
        return;
    }

    // Wait for the spill buffer to clear
    if (ref_window_ < (spill_buffer_ + config_->tRFC))
    {
        return;
    }

    // Recv are spread out
    if (clk_ - last_recv_req_clk_ < recv_req_dist_)
    {
        return;
    }

    // We only need RAAIMT/2 acts in a window
    if (recv_window_reqs_ >= config_->ref_raa_decrement)
    {
        return;
    }


    uint64_t addr_ = recv_addr_ + recv_row_idx_ * offset_;
    if (memory_system_.WillAcceptTransaction(addr_, false))
    {
    // std::cout << "[Recv] window_idx = " << ref_window_idx_ << " last_recv_window_idx_ = " << last_recv_window_idx_ << " recv_window_reqs_ = " << recv_window_reqs_ << std::endl;
    // std::cout << "[Recv] clk_ = " << clk_ << " last_recv_req_clk_ = " << last_recv_req_clk_ << " recv_req_dist_ = " << recv_req_dist_ << std::endl; 
        memory_system_.AddTransaction(addr_, false);
        recv_row_idx_ = (recv_row_idx_ + 1) % max_rows_;
        recv_window_reqs_++;
        last_recv_req_clk_ = clk_;
    }
}

void CovChCPU::ClockTick()
{
    memory_system_.ClockTick();

    ref_window_ = clk_ % config_->tREFI;
    ref_window_idx_ = clk_ / config_->tREFI;

    if (clk_ < config_->tREFI * warmup_idx_)
    {
        // Warmup
    }
    else
    {
        TickSend();
        TickRecv();
    }
    prev_window_idx_ = ref_window_idx_;
    clk_++;
    return;
}

}  // namespace dramsim3
