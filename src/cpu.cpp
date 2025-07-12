#include "cpu.hpp"
#include "memory.hpp"
#include "registers.hpp"
#include <cstdint>
#include <format>
#include <functional>
#include <iostream>

CPU::CPU(Memory& memory)
    : memory(memory)
{
    regs.af.set(0x01b0);
    regs.bc.set(0x0013);
    regs.de.set(0x00d8);
    regs.hl.set(0x014d);
    regs.pc = 0x0100;
    regs.sp = 0xfffe;

    setup_tables();
}

void CPU::cycle()
{
    if (stopped) {
        handle_timers();
        return;
    }

    if (halted) {
        if (interrupt_pending()) {
            halted = false;
        } else {
            handle_timers();
            return;
        }
    }

    if (cycles_left == 0) {
        if (halt_bug) {
            opcode = memory.read_byte(regs.pc);
            halt_bug = false;
        } else {
            opcode = memory.read_byte(regs.pc++);
        }
        decode_and_execute();
        cycles_left = instruction_cycles[opcode] - 1;
    } else {
        --cycles_left;
    }

    if (ime_next) {
        ime = true;
        ime_next = false;
    }

    handle_interrupts();
    handle_timers();
    // check_serial_output();
}

bool CPU::interrupt_pending()
{
    return (memory.read_byte(Memory::IE_ADDR) & memory.read_byte(Memory::IF_ADDR)) != 0x0;
}

void CPU::handle_interrupts()
{
    if (!ime)
        return;

    uint8_t ie = memory.read_byte(Memory::IE_ADDR);
    uint8_t iflag = memory.read_byte(Memory::IF_ADDR);
    uint8_t triggered = ie & iflag;

    if (triggered == 0)
        return;

    for (int i = 0; i < 5; ++i) {
        if (triggered & (1 << i)) {
            ime = false;

            iflag &= ~(1 << i);
            memory.write_byte(Memory::IF_ADDR, iflag);

            uint16_t pc = regs.pc;
            regs.sp -= 2;
            memory.write_byte(regs.sp, lsb(pc));
            memory.write_byte(regs.sp + 1, msb(pc));

            regs.pc = 0x40 + i * 0x08;

            cycles_left += 20;

            return;
        }
    }
}

void CPU::handle_timers()
{
    total_cycles++;

    if ((total_cycles & 0xFF) == 0) {
        uint8_t div = memory.read_byte(0xFF04);
        memory.write_byte(0xFF04, div + 1);
    }

    uint8_t tac = memory.read_byte(0xFF07);
    if (tac & 0x04) { // Timer enabled
        uint16_t mask;
        switch (tac & 0x03) {
        case 0:
            mask = 0x3FF;
            break; // 1024 cycles
        case 1:
            mask = 0x0F;
            break; // 16 cycles
        case 2:
            mask = 0x3F;
            break; // 64 cycles
        case 3:
            mask = 0xFF;
            break; // 256 cycles
        }

        if ((total_cycles & mask) == 0) {
            uint8_t tima = memory.read_byte(0xFF05);
            if (tima == 0xFF) {
                memory.write_byte(0xFF05, memory.read_byte(0xFF06));
                uint8_t iflag = memory.read_byte(0xFF0F);
                memory.write_byte(0xFF0F, iflag | 0x04);
            } else {
                memory.write_byte(0xFF05, tima + 1);
            }
        }
    }
}

void CPU::check_serial_output()
{
    uint8_t sc = memory.read_byte(0xFF02);
    if (sc & 0x80) {
        uint8_t sb = memory.read_byte(0xFF01);
        char c = static_cast<char>(sb);
        std::cout << c;
        sc &= ~0x80;
        memory.write_byte(0xFF02, sc);
        uint8_t iflag = memory.read_byte(Memory::IF_ADDR);
        memory.write_byte(Memory::IF_ADDR, iflag | 0x08);
    }
}

void CPU::setup_tables()
{
    std::array<std::function<uint8_t&()>, 8> reg8_getters = {
        [&]() -> uint8_t& { return regs.b(); },
        [&]() -> uint8_t& { return regs.c(); },
        [&]() -> uint8_t& { return regs.d(); },
        [&]() -> uint8_t& { return regs.e(); },
        [&]() -> uint8_t& { return regs.h(); },
        [&]() -> uint8_t& { return regs.l(); },
        [&]() -> uint8_t& { return memory.at(regs.hl.get()); },
        [&]() -> uint8_t& { return regs.a(); }
    };

    opcode_table[0xcb] = [this]() {
        uint8_t cb_opcode = fetch_byte();
        cbcode_table[cb_opcode]();
        cycles_left += cb_instruction_cycles[cb_opcode];
    };
    instruction_cycles[0xcb] = 4;
    opcode_table[0x00] = [this]() { op_nop(); };
    instruction_cycles[0x00] = 4;
    opcode_table[0x10] = [this]() { op_stop(); };
    instruction_cycles[0x10] = 4;
    opcode_table[0x76] = [this]() { op_halt(); };
    instruction_cycles[0x76] = 4;
    opcode_table[0xf3] = [this]() { op_di(); };
    instruction_cycles[0xf3] = 4;
    opcode_table[0xfb] = [this]() { op_ei(); };
    instruction_cycles[0xfb] = 4;

    opcode_table[0x03] = [this]() { inc_reg(regs.bc); };
    instruction_cycles[0x03] = 8;
    opcode_table[0x13] = [this]() { inc_reg(regs.de); };
    instruction_cycles[0x13] = 8;
    opcode_table[0x23] = [this]() { inc_reg(regs.hl); };
    instruction_cycles[0x23] = 8;
    opcode_table[0x33] = [this]() { op_inc_sp(); };
    instruction_cycles[0x33] = 8;

    for (uint8_t i = 0x0; i < 0x8; ++i) {
        uint8_t opcode = 0x04 + i * 0x08;
        instruction_cycles[opcode] = 4;
        if (opcode == 0x34) {
            opcode_table[opcode] = [this]() { op_inc__hl_(); };
            instruction_cycles[opcode] += 8;
        } else {
            opcode_table[opcode] = [this, reg8_getters, i]() {
                inc_reg(reg8_getters[i]());
            };
        };
    }

    opcode_table[0x0b] = [this]() { dec_reg(regs.bc); };
    instruction_cycles[0x0b] = 8;
    opcode_table[0x1b] = [this]() { dec_reg(regs.de); };
    instruction_cycles[0x1b] = 8;
    opcode_table[0x2b] = [this]() { dec_reg(regs.hl); };
    instruction_cycles[0x2b] = 8;
    opcode_table[0x3b] = [this]() { op_dec_sp(); };
    instruction_cycles[0x3b] = 8;

    for (uint8_t i = 0x0; i < 0x8; ++i) {
        uint8_t opcode = 0x05 + i * 0x08;
        instruction_cycles[opcode] = 4;
        if (opcode == 0x35) {
            opcode_table[opcode] = [this]() { op_dec__hl_(); };
            instruction_cycles[opcode] += 8;
        } else {
            opcode_table[opcode] = [this, reg8_getters, i]() {
                dec_reg(reg8_getters[i]());
            };
        };
    }

    opcode_table[0x09] = [this]() { add_hl_reg16(regs.bc.get()); };
    instruction_cycles[0x09] = 8;
    opcode_table[0x19] = [this]() { add_hl_reg16(regs.de.get()); };
    instruction_cycles[0x19] = 8;
    opcode_table[0x29] = [this]() { add_hl_reg16(regs.hl.get()); };
    instruction_cycles[0x29] = 8;
    opcode_table[0x39] = [this]() { add_hl_reg16(regs.sp); };
    instruction_cycles[0x39] = 8;

    opcode_table[0xc3] = [this]() { op_jp_a16(); };
    instruction_cycles[0xc3] = 16;
    opcode_table[0xe9] = [this]() { op_jp__hl_(); };
    instruction_cycles[0xe9] = 4;
    opcode_table[0xc2] = [this]() { op_jp_nz_a16(); };
    instruction_cycles[0xc2] = 12;
    opcode_table[0xd2] = [this]() { op_jp_nc_a16(); };
    instruction_cycles[0xd2] = 12;
    opcode_table[0xca] = [this]() { op_jp_z_a16(); };
    instruction_cycles[0xca] = 12;
    opcode_table[0xda] = [this]() { op_jp_c_a16(); };
    instruction_cycles[0xda] = 12;

    opcode_table[0x18] = [this]() { op_jr_r8(); };
    instruction_cycles[0x18] = 12;
    opcode_table[0x20] = [this]() { op_jr_nz_r8(); };
    instruction_cycles[0x20] = 12;
    opcode_table[0x30] = [this]() { op_jr_nc_r8(); };
    instruction_cycles[0x30] = 12;
    opcode_table[0x28] = [this]() { op_jr_z_r8(); };
    instruction_cycles[0x28] = 12;
    opcode_table[0x38] = [this]() { op_jr_c_r8(); };
    instruction_cycles[0x38] = 12;

    opcode_table[0xcd] = [this]() { op_call_a16(); };
    instruction_cycles[0xcd] = 24;
    opcode_table[0xc4] = [this]() { op_call_nz_a16(); };
    instruction_cycles[0xc4] = 12;
    opcode_table[0xd4] = [this]() { op_call_nc_a16(); };
    instruction_cycles[0xd4] = 12;
    opcode_table[0xcc] = [this]() { op_call_z_a16(); };
    instruction_cycles[0xcc] = 12;
    opcode_table[0xdc] = [this]() { op_call_c_a16(); };
    instruction_cycles[0xdc] = 12;

    opcode_table[0xc9] = [this]() { op_ret(); };
    instruction_cycles[0xc9] = 16;
    opcode_table[0xd9] = [this]() { op_reti(); };
    instruction_cycles[0xd9] = 16;
    opcode_table[0xc0] = [this]() { op_ret_nz(); };
    instruction_cycles[0xc0] = 8;
    opcode_table[0xd0] = [this]() { op_ret_nc(); };
    instruction_cycles[0xd0] = 8;
    opcode_table[0xc8] = [this]() { op_ret_z(); };
    instruction_cycles[0xc8] = 8;
    opcode_table[0xd8] = [this]() { op_ret_c(); };
    instruction_cycles[0xd8] = 8;

    for (uint8_t i = 0x0; i < 0x8; ++i) {
        uint8_t opcode = 0xc7 + i * 0x08;
        opcode_table[opcode] = [this, i]() {
            call_to(((i / 2) << 3) | (i % 2) * 0x8);
        };
        instruction_cycles[opcode] = 16;
    }

    opcode_table[0x01] = [this]() { op_ld_bc_d16(); };
    instruction_cycles[0x01] = 12;
    opcode_table[0x11] = [this]() { op_ld_de_d16(); };
    instruction_cycles[0x11] = 12;
    opcode_table[0x3d] = [this]() { dec_reg(regs.a()); };
    instruction_cycles[0x3d] = 4;
    opcode_table[0x21] = [this]() { op_ld_hl_d16(); };
    instruction_cycles[0x21] = 12;
    opcode_table[0x31] = [this]() { op_ld_sp_d16(); };
    instruction_cycles[0x31] = 12;
    opcode_table[0x08] = [this]() { op_ld__a16__sp(); };
    instruction_cycles[0x08] = 20;
    opcode_table[0xf8] = [this]() { op_ld_hl_sp_r8(); };
    instruction_cycles[0xf8] = 12;
    opcode_table[0xf9] = [this]() { op_ld_sp_hl(); };
    instruction_cycles[0xf9] = 8;

    for (uint8_t i = 0x0; i < 0x8; ++i) {
        uint8_t opcode = 0x06 + i * 0x08;
        instruction_cycles[opcode] = 8;
        if (opcode == 0x35) {
            opcode_table[opcode] = [this]() { op_ld__hl__d8(); };
            instruction_cycles[opcode] += 4;
        } else {
            opcode_table[opcode] = [this, reg8_getters, i]() {
                ld_reg8_d8(reg8_getters[i]());
            };
        };
    }

    for (uint8_t dst = 0x0; dst < 0x8; ++dst) {
        for (uint8_t src = 0x0; src < 0x8; ++src) {
            uint8_t opcode = 0x40 | dst << 3 | src;
            if (opcode == 0x76)
                continue;

            uint8_t dst_copy = dst;
            uint8_t src_copy = src;
            if (0x70 <= opcode and opcode < 0x78) {
                opcode_table[opcode] = [this, reg8_getters, src_copy]() {
                    ld__hl__r8(reg8_getters[src_copy]());
                };
                instruction_cycles[opcode] = 8;
            } else {
                opcode_table[opcode] = [this, reg8_getters, dst_copy, src_copy]() {
                    ld_r8_r8(reg8_getters[dst_copy](), reg8_getters[src_copy]());
                };
                instruction_cycles[opcode] = src == 0x6 ? 8 : 4;
            }
        }
    }

    opcode_table[0x02] = [this]() { op_ld__bc__a(); };
    instruction_cycles[0x02] = 8;
    opcode_table[0x12] = [this]() { op_ld__de__a(); };
    instruction_cycles[0x12] = 8;
    opcode_table[0x22] = [this]() { op_ld__hlp__a(); };
    instruction_cycles[0x22] = 8;
    opcode_table[0x32] = [this]() { op_ld__hlm__a(); };
    instruction_cycles[0x32] = 8;

    opcode_table[0x0a] = [this]() { op_ld_a__bc_(); };
    instruction_cycles[0x0a] = 8;
    opcode_table[0x1a] = [this]() { op_ld_a__de_(); };
    instruction_cycles[0x1a] = 8;
    opcode_table[0x2a] = [this]() { op_ld_a__hlp_(); };
    instruction_cycles[0x2a] = 8;
    opcode_table[0x3a] = [this]() { op_ld_a__hlm_(); };
    instruction_cycles[0x3a] = 8;

    opcode_table[0xea] = [this]() { op_ld__a16__a(); };
    instruction_cycles[0xea] = 16;
    opcode_table[0xfa] = [this]() { op_ld_a__a16_(); };
    instruction_cycles[0xfa] = 16;
    opcode_table[0xe0] = [this]() { op_ldh__a8__a(); };
    instruction_cycles[0xe0] = 12;
    opcode_table[0xf0] = [this]() { op_ldh_a__a8_(); };
    instruction_cycles[0xf0] = 12;
    opcode_table[0xe2] = [this]() { op_ld__c__a(); };
    instruction_cycles[0xe2] = 8;
    opcode_table[0xf2] = [this]() { op_ld_a__c_(); };
    instruction_cycles[0xf2] = 8;

    opcode_table[0xc5] = [this]() { push_reg(regs.bc); };
    instruction_cycles[0xc5] = 16;
    opcode_table[0xd5] = [this]() { push_reg(regs.de); };
    instruction_cycles[0xd5] = 16;
    opcode_table[0xe5] = [this]() { push_reg(regs.hl); };
    instruction_cycles[0xe5] = 16;
    opcode_table[0xf5] = [this]() { push_reg(regs.af); };
    instruction_cycles[0xf5] = 16;

    opcode_table[0xc1] = [this]() { pop_reg(regs.bc); };
    instruction_cycles[0xc1] = 12;
    opcode_table[0xd1] = [this]() { pop_reg(regs.de); };
    instruction_cycles[0xd1] = 12;
    opcode_table[0xe1] = [this]() { pop_reg(regs.hl); };
    instruction_cycles[0xe1] = 12;
    opcode_table[0xf1] = [this]() { pop_reg(regs.af, true); };
    instruction_cycles[0xf1] = 12;

    register_reg8_manip_group(0x80, reg8_getters, [this](uint8_t& r) { add_reg_update_flags(r); });
    register_reg8_manip_group(0x88, reg8_getters, [this](uint8_t& r) { add_reg_update_flags(r, true); });
    opcode_table[0xc6] = [this]() { op_add_d8(); };
    instruction_cycles[0xc6] = 8;
    opcode_table[0xce] = [this]() { op_adc_d8(); };
    instruction_cycles[0xce] = 8;

    register_reg8_manip_group(0x90, reg8_getters, [this](uint8_t& r) { sub_reg_update_flags(r); });
    register_reg8_manip_group(0x98, reg8_getters, [this](uint8_t& r) { sub_reg_update_flags(r, true); });
    opcode_table[0xd6] = [this]() { op_sub_d8(); };
    instruction_cycles[0xd6] = 8;
    opcode_table[0xde] = [this]() { op_sbc_d8(); };
    instruction_cycles[0xde] = 8;

    register_reg8_manip_group(0xa0, reg8_getters, [this](uint8_t& r) { and_reg_update_flags(r); });
    opcode_table[0xe6] = [this]() { op_and_d8(); };
    instruction_cycles[0xe6] = 8;

    register_reg8_manip_group(0xa8, reg8_getters, [this](uint8_t& r) { xor_reg_update_flags(r); });
    opcode_table[0xee] = [this]() { op_xor_d8(); };
    instruction_cycles[0xee] = 8;

    register_reg8_manip_group(0xb0, reg8_getters, [this](uint8_t& r) { or_reg_update_flags(r); });
    opcode_table[0xf6] = [this]() { op_or_d8(); };
    instruction_cycles[0xf6] = 8;

    register_reg8_manip_group(0xb8, reg8_getters, [this](uint8_t& r) { cp_reg_update_flags(r); });
    opcode_table[0xfe] = [this]() { op_cp_d8(); };
    instruction_cycles[0xfe] = 8;

    opcode_table[0x07] = [this]() { op_rlca(); };
    instruction_cycles[0x07] = 4;
    opcode_table[0x17] = [this]() { op_rla(); };
    instruction_cycles[0x17] = 4;
    opcode_table[0x0f] = [this]() { op_rrca(); };
    instruction_cycles[0x0f] = 4;
    opcode_table[0x1f] = [this]() { op_rra(); };
    instruction_cycles[0x1f] = 4;

    opcode_table[0x3f] = [this]() { op_ccf(); };
    instruction_cycles[0x3f] = 4;
    opcode_table[0x37] = [this]() { op_scf(); };
    instruction_cycles[0x37] = 4;
    opcode_table[0x27] = [this]() { op_daa(); };
    instruction_cycles[0x27] = 4;
    opcode_table[0x2f] = [this]() { op_cpl(); };
    instruction_cycles[0x2f] = 4;

    opcode_table[0xe8] = [this]() { op_add_sp_r8(); };
    instruction_cycles[0xe8] = 16;

    register_cb_group(0x00, reg8_getters, [this](uint8_t& r) { op_rlc_reg8(r); }, [this]() { op_rlc__hl_(); });
    register_cb_group(0x08, reg8_getters, [this](uint8_t& r) { op_rrc_reg8(r); }, [this]() { op_rrc__hl_(); });
    register_cb_group(0x10, reg8_getters, [this](uint8_t& r) { op_rl_reg8(r); }, [this]() { op_rl__hl_(); });
    register_cb_group(0x18, reg8_getters, [this](uint8_t& r) { op_rr_reg8(r); }, [this]() { op_rr__hl_(); });
    register_cb_group(0x20, reg8_getters, [this](uint8_t& r) { op_sla_reg8(r); }, [this]() { op_sla__hl_(); });
    register_cb_group(0x28, reg8_getters, [this](uint8_t& r) { op_sra_reg8(r); }, [this]() { op_sra__hl_(); });
    register_cb_group(0x30, reg8_getters, [this](uint8_t& r) { op_swap_reg8(r); }, [this]() { op_swap__hl_(); });
    register_cb_group(0x38, reg8_getters, [this](uint8_t& r) { op_srl_reg8(r); }, [this]() { op_srl__hl_(); });

    register_cb_group_large(
        0x40,
        reg8_getters,
        [this](const uint8_t r, const uint8_t bit) { bit_b_reg8(r, bit); },
        [this](const uint8_t bit) { op_bit_b__hl_(bit); });
    register_cb_group_large(
        0x80,
        reg8_getters,
        [this](uint8_t& r, const uint8_t bit) { res_b_reg8(r, bit); },
        [this](const uint8_t bit) { op_res_b__hl_(bit); });
    register_cb_group_large(
        0xc0,
        reg8_getters,
        [this](uint8_t& r, const uint8_t bit) { set_b_reg8(r, bit); },
        [this](const uint8_t bit) { op_set_b__hl_(bit); });
}

void CPU::register_reg8_manip_group(uint8_t start_code,
    const std::array<std::function<uint8_t&()>, 8>& reg8_getters,
    std::function<void(uint8_t&)> fn)
{
    for (uint8_t i = 0x0; i < 0x8; ++i) {
        uint8_t opcode = start_code + i;
        instruction_cycles[opcode] = 4;
        opcode_table[opcode] = [this, reg8_getters, fn, i]() {
            fn(reg8_getters[i]());
        };
        if (i == 6)
            instruction_cycles[opcode] += 4;
    }
}

void CPU::register_cb_group(uint8_t start_code,
    const std::array<std::function<uint8_t&()>, 8>& reg8_getters,
    std::function<void(uint8_t&)> reg8_fn,
    std::function<void()> hl_fn)
{
    for (uint8_t i = 0; i < 0x8; ++i) {
        uint8_t opcode = start_code + i;
        cb_instruction_cycles[opcode] = 8;
        if (i == 6) {
            cbcode_table[opcode] = [this, hl_fn]() {
                hl_fn();
            };
            cb_instruction_cycles[opcode] += 8;
        } else {
            cbcode_table[opcode] = [this, reg8_fn, reg8_getters, i]() {
                reg8_fn(reg8_getters[i]());
            };
        }
    }
}

template <typename Reg8Fn>
void CPU::register_cb_group_large(uint8_t start_code,
    const std::array<std::function<uint8_t&()>, 8>& reg8_getters,
    Reg8Fn reg8_fn,
    std::function<void(const uint8_t)> hl_fn)
{
    for (uint8_t bit = 0; bit < 0x8; ++bit) {
        for (uint8_t i = 0; i < 0x8; ++i) {
            uint8_t opcode = start_code + i + bit * 0x8;
            cb_instruction_cycles[opcode] = 8;
            if (i == 6) {
                cbcode_table[opcode] = [this, hl_fn, bit]() {
                    hl_fn(bit);
                };
                cb_instruction_cycles[opcode] += 8;
            } else {
                cbcode_table[opcode] = [this, reg8_fn, reg8_getters, i, bit]() {
                    reg8_fn(reg8_getters[i](), bit);
                };
            }
        }
    }
}

uint8_t CPU::fetch_byte()
{
    return memory.read_byte(regs.pc++);
}

uint16_t CPU::fetch_word()
{
    return build_word(fetch_byte(), fetch_byte());
}

void CPU::decode_and_execute()
{
    if (opcode_table.contains(opcode))
        opcode_table[opcode]();
    else
        throw std::runtime_error(std::format("Unknown opcode: 0x{:X}", opcode));
}

void CPU::op_nop() { }

void CPU::op_stop()
{
    stopped = true;
}

void CPU::op_halt()
{
    if (!ime && interrupt_pending())
        halt_bug = true;
    else
        halted = true;
}

void CPU::op_di()
{
    ime = false;
}

void CPU::op_ei()
{
    ime_next = true;
}

inline void CPU::inc_reg(RegisterPair& reg)
{
    reg.set(reg.get() + 1);
}

void CPU::op_inc_sp()
{
    regs.sp++;
}

inline void CPU::inc_reg(uint8_t& reg)
{
    reg = inc_reg8_update_flags(reg);
}

inline uint8_t CPU::inc_reg8_update_flags(uint8_t reg_val)
{
    uint8_t res = reg_val + 1;
    regs.set_flag(Registers::FLAG_Z, res == 0);
    regs.set_flag(Registers::FLAG_N, false);
    regs.set_flag(Registers::FLAG_H, ((reg_val & 0x0f) + 1) > 0x0f);
    return res;
}

void CPU::op_inc__hl_()
{
    memory.write_byte(regs.hl.get(), inc_reg8_update_flags(memory.read_byte(regs.hl.get())));
}

inline void CPU::dec_reg(RegisterPair& reg)
{
    reg.set(reg.get() - 1);
}

void CPU::op_dec_sp()
{
    regs.sp--;
}

inline void CPU::dec_reg(uint8_t& reg)
{
    reg = dec_reg8_update_flags(reg);
}

inline uint8_t CPU::dec_reg8_update_flags(uint8_t reg_val)
{

    uint8_t res = reg_val - 1;
    regs.set_flag(Registers::FLAG_Z, res == 0);
    regs.set_flag(Registers::FLAG_N, true);
    regs.set_flag(Registers::FLAG_H, (reg_val & 0x0f) == 0x00);
    return res;
}

void CPU::op_dec__hl_()
{
    memory.write_byte(regs.hl.get(), dec_reg8_update_flags(memory.read_byte(regs.hl.get())));
}

inline void CPU::add_hl_reg16(uint16_t value)
{
    uint16_t hl = regs.hl.get();
    uint32_t result = static_cast<uint32_t>(hl) + value;
    regs.hl.set(static_cast<uint16_t>(result));
    regs.set_flag(Registers::FLAG_N, false);
    regs.set_flag(Registers::FLAG_H, ((hl & 0x0FFF) + (value & 0x0FFF)) > 0x0FFF);
    regs.set_flag(Registers::FLAG_C, result > 0xFFFF);
}

inline void CPU::jp_to(uint16_t addr)
{

    regs.pc = addr;
}

inline void CPU::conditional_jp_to(uint16_t addr, bool condition)
{
    if (condition) {
        jp_to(addr);
        cycles_left += 4;
    }
}

void CPU::op_jp_a16()
{
    jp_to(fetch_word());
}

void CPU::op_jp__hl_()
{
    jp_to(regs.hl.get());
}

void CPU::op_jp_nz_a16()
{
    conditional_jp_to(fetch_word(), !regs.get_flag(Registers::FLAG_Z));
}

void CPU::op_jp_nc_a16()
{
    conditional_jp_to(fetch_word(), !regs.get_flag(Registers::FLAG_C));
}

void CPU::op_jp_z_a16()
{
    conditional_jp_to(fetch_word(), regs.get_flag(Registers::FLAG_Z));
}

void CPU::op_jp_c_a16()
{
    conditional_jp_to(fetch_word(), regs.get_flag(Registers::FLAG_C));
}

inline void CPU::jr_of(uint8_t unsigned_offset)
{
    regs.pc += static_cast<int8_t>(unsigned_offset);
}

inline void CPU::conditional_jr_of(uint8_t unsigned_offset, bool condition)
{
    if (condition) {
        jr_of(unsigned_offset);
        cycles_left += 4;
    }
}

void CPU::op_jr_r8()
{
    jr_of(fetch_byte());
}

void CPU::op_jr_nz_r8()
{
    conditional_jr_of(fetch_byte(), !regs.get_flag(Registers::FLAG_Z));
}

void CPU::op_jr_nc_r8()
{
    conditional_jr_of(fetch_byte(), !regs.get_flag(Registers::FLAG_C));
}

void CPU::op_jr_z_r8()
{
    conditional_jr_of(fetch_byte(), regs.get_flag(Registers::FLAG_Z));
}

void CPU::op_jr_c_r8()
{
    conditional_jr_of(fetch_byte(), regs.get_flag(Registers::FLAG_C));
}

inline void CPU::call_to(uint16_t addr)
{
    uint16_t ret = regs.pc;
    memory.write_byte(--regs.sp, msb(ret));
    memory.write_byte(--regs.sp, lsb(ret));
    regs.pc = addr;
}

inline void CPU::conditional_call_to(uint16_t addr, bool condition)
{
    if (condition) {
        call_to(addr);
        cycles_left += 12;
    }
}

void CPU::op_call_a16()
{
    call_to(fetch_word());
}

void CPU::op_call_nz_a16()
{
    conditional_call_to(fetch_word(), !regs.get_flag(Registers::FLAG_Z));
}

void CPU::op_call_nc_a16()
{
    conditional_call_to(fetch_word(), !regs.get_flag(Registers::FLAG_C));
}

void CPU::op_call_z_a16()
{
    conditional_call_to(fetch_word(), regs.get_flag(Registers::FLAG_Z));
}

void CPU::op_call_c_a16()
{
    conditional_call_to(fetch_word(), regs.get_flag(Registers::FLAG_C));
}

void CPU::op_ret()
{
    uint8_t lsb = memory.read_byte(regs.sp++);
    uint8_t msb = memory.read_byte(regs.sp++);
    regs.pc = build_word(lsb, msb);
}

void CPU::op_reti()
{
    op_ret();
    ime = 1;
}

inline void CPU::conditional_ret(bool condition)
{
    if (condition) {
        op_ret();
        cycles_left += 12;
    }
}

void CPU::op_ret_nz()
{
    conditional_ret(!regs.get_flag(Registers::FLAG_Z));
}

void CPU::op_ret_nc()
{
    conditional_ret(!regs.get_flag(Registers::FLAG_C));
}

void CPU::op_ret_z()
{
    conditional_ret(regs.get_flag(Registers::FLAG_Z));
}

void CPU::op_ret_c()
{
    conditional_ret(regs.get_flag(Registers::FLAG_C));
}

void CPU::op_ld_bc_d16()
{
    regs.bc.set(fetch_word());
}

void CPU::op_ld_de_d16()
{
    regs.de.set(fetch_word());
}

void CPU::op_ld_hl_d16()
{
    regs.hl.set(fetch_word());
}

void CPU::op_ld_sp_d16()
{
    regs.sp = fetch_word();
}

void CPU::op_ld__a16__sp()
{
    uint16_t addr = fetch_word();
    memory.write_byte(addr, lsb(regs.sp));
    memory.write_byte(addr + 1, msb(regs.sp));
}

void CPU::op_ld_hl_sp_r8()
{
    int8_t e = static_cast<int8_t>(fetch_byte());
    regs.set_flag(Registers::FLAG_Z, false);
    regs.set_flag(Registers::FLAG_N, false);
    regs.set_flag(Registers::FLAG_H, (e & 0xf) + (regs.sp & 0xf) > 0xf);
    regs.set_flag(Registers::FLAG_C, (e & 0xff) + (regs.sp & 0xff) > 0xff);
    regs.hl.set(regs.sp + e);
}

void CPU::op_ld_sp_hl()
{
    regs.sp = regs.hl.get();
}

inline void CPU::ld_reg8_d8(uint8_t& reg)
{
    reg = fetch_byte();
}

void CPU::op_ld__hl__d8()
{
    memory.write_byte(regs.hl.get(), fetch_byte());
}

inline void CPU::ld_r8_r8(uint8_t& dst, const uint8_t value)
{
    dst = value;
}

inline void CPU::ld__hl__r8(const uint8_t value)
{
    memory.write_byte(regs.hl.get(), value);
}

void CPU::op_ld__bc__a()
{
    memory.write_byte(regs.bc.get(), regs.a());
}

void CPU::op_ld__de__a()
{
    memory.write_byte(regs.de.get(), regs.a());
}

void CPU::op_ld__hlp__a()
{
    memory.write_byte(regs.hl.get()++, regs.a());
}

void CPU::op_ld__hlm__a()
{
    memory.write_byte(regs.hl.get()--, regs.a());
}

void CPU::op_ld_a__bc_()
{
    regs.a() = memory.read_byte(regs.bc.get());
}

void CPU::op_ld_a__de_()
{
    regs.a() = memory.read_byte(regs.de.get());
}

void CPU::op_ld_a__hlp_()
{
    regs.a() = memory.read_byte(regs.hl.get()++);
}

void CPU::op_ld_a__hlm_()
{
    regs.a() = memory.read_byte(regs.hl.get()--);
}

void CPU::op_ld__a16__a()
{
    memory.write_byte(fetch_word(), regs.a());
}

void CPU::op_ld_a__a16_()
{
    regs.a() = memory.read_byte(fetch_word());
}

void CPU::op_ldh__a8__a()
{
    memory.write_byte(build_word(fetch_byte(), 0xff), regs.a());
}

void CPU::op_ldh_a__a8_()
{
    regs.a() = memory.read_byte(build_word(fetch_byte(), 0xff));
}

void CPU::op_ld__c__a()
{
    memory.write_byte(build_word(regs.c(), 0xff), regs.a());
}

void CPU::op_ld_a__c_()
{
    regs.a() = memory.read_byte(build_word(regs.c(), 0xff));
}

inline void CPU::push_reg(RegisterPair& reg)
{

    memory.write_byte(--regs.sp, msb(reg.get()));
    memory.write_byte(--regs.sp, lsb(reg.get()));
}

inline void CPU::pop_reg(RegisterPair& reg, bool clear_lower_4bits)
{

    uint8_t lsb = memory.read_byte(regs.sp++);
    if (clear_lower_4bits)
        lsb &= 0xF0;
    uint8_t msb = memory.read_byte(regs.sp++);
    reg.set(build_word(lsb, msb));
}

inline void CPU::add_reg_update_flags(uint8_t reg_val, bool use_carry)
{
    uint8_t carry = regs.get_flag(Registers::FLAG_C) * use_carry;
    uint16_t result = regs.a() + reg_val + carry;
    regs.set_flag(Registers::FLAG_Z, (result & 0xff) == 0);
    regs.set_flag(Registers::FLAG_N, false);
    regs.set_flag(Registers::FLAG_H, (regs.a() & 0xf) + (reg_val & 0xf) + carry > 0xf);
    regs.set_flag(Registers::FLAG_C, result > 0xff);
    regs.a() = static_cast<uint8_t>(result);
}

void CPU::op_add_d8()
{
    add_reg_update_flags(fetch_byte());
}
void CPU::op_adc_d8()
{
    add_reg_update_flags(fetch_byte(), true);
}

inline void CPU::sub_reg_update_flags(uint8_t reg_val, bool use_carry)
{
    uint8_t carry = (use_carry and regs.get_flag(Registers::FLAG_C)) ? 1 : 0;
    uint16_t sub_total = reg_val + carry;
    uint16_t result = regs.a() - sub_total;

    regs.set_flag(Registers::FLAG_Z, (result & 0xFF) == 0);
    regs.set_flag(Registers::FLAG_N, true);
    regs.set_flag(Registers::FLAG_H, ((regs.a() ^ reg_val ^ result) & 0x10) != 0);
    regs.set_flag(Registers::FLAG_C, result > 0xFF);

    regs.a() = static_cast<uint8_t>(result & 0xFF);
}

void CPU::op_sub_d8()
{
    sub_reg_update_flags(fetch_byte());
}

void CPU::op_sbc_d8()
{
    sub_reg_update_flags(fetch_byte(), true);
}

inline void CPU::and_reg_update_flags(uint8_t reg_val)
{
    uint8_t result = regs.a() & reg_val;
    regs.a() = result;
    regs.set_flag(Registers::FLAG_Z, result == 0);
    regs.set_flag(Registers::FLAG_N, false);
    regs.set_flag(Registers::FLAG_H, true);
    regs.set_flag(Registers::FLAG_C, false);
}

void CPU::op_and_d8()
{
    and_reg_update_flags(fetch_byte());
}

inline void CPU::xor_reg_update_flags(uint8_t reg_val)
{
    uint8_t result = regs.a() ^ reg_val;
    regs.a() = result;
    regs.set_flag(Registers::FLAG_Z, result == 0);
    regs.set_flag(Registers::FLAG_N, false);
    regs.set_flag(Registers::FLAG_H, false);
    regs.set_flag(Registers::FLAG_C, false);
}

void CPU::op_xor_d8()
{
    xor_reg_update_flags(fetch_byte());
}

inline void CPU::or_reg_update_flags(uint8_t reg_val)
{
    uint8_t result = regs.a() | reg_val;
    regs.a() = result;
    regs.set_flag(Registers::FLAG_Z, result == 0);
    regs.set_flag(Registers::FLAG_N, false);
    regs.set_flag(Registers::FLAG_H, false);
    regs.set_flag(Registers::FLAG_C, false);
}

void CPU::op_or_d8()
{
    or_reg_update_flags(fetch_byte());
}

inline void CPU::cp_reg_update_flags(uint8_t reg_val)
{
    uint8_t result = regs.a() - reg_val;
    regs.set_flag(Registers::FLAG_Z, result == 0);
    regs.set_flag(Registers::FLAG_N, 1);
    regs.set_flag(Registers::FLAG_H, (regs.a() & 0xf) < (reg_val & 0xf));
    regs.set_flag(Registers::FLAG_C, regs.a() < reg_val);
}

void CPU::op_cp_d8()
{
    cp_reg_update_flags(fetch_byte());
}

inline void CPU::update_rotate_flags(bool c)
{
    regs.set_flag(Registers::FLAG_Z, false);
    regs.set_flag(Registers::FLAG_N, false);
    regs.set_flag(Registers::FLAG_H, false);
    regs.set_flag(Registers::FLAG_C, c);
}

void CPU::op_rlca()
{
    bool b7 = (regs.a() >> 7) & 0x1;
    regs.a() = (regs.a() << 1) | b7;
    update_rotate_flags(b7);
}

void CPU::op_rla()
{
    bool b7 = (regs.a() >> 7) & 0x1;
    regs.a() = (regs.a() << 1) | (regs.get_flag(Registers::FLAG_C));
    update_rotate_flags(b7);
}

void CPU::op_rrca()
{
    bool b0 = regs.a() & 0x1;
    regs.a() = (regs.a() >> 1) | (b0 << 7);
    update_rotate_flags(b0);
}

void CPU::op_rra()
{
    bool b0 = regs.a() & 0x1;
    regs.a() = (regs.a() >> 1) | (regs.get_flag(Registers::FLAG_C) << 7);
    update_rotate_flags(b0);
}

void CPU::op_ccf()
{
    regs.set_flag(Registers::FLAG_N, false);
    regs.set_flag(Registers::FLAG_H, false);
    regs.set_flag(Registers::FLAG_C, !regs.get_flag(Registers::FLAG_C));
}

void CPU::op_scf()
{
    regs.set_flag(Registers::FLAG_N, false);
    regs.set_flag(Registers::FLAG_H, false);
    regs.set_flag(Registers::FLAG_C, true);
}

void CPU::op_daa()
{
    uint8_t correction = 0;
    bool carry_flag = regs.get_flag(Registers::FLAG_C);

    if (!regs.get_flag(Registers::FLAG_N)) { // After addition
        if (regs.get_flag(Registers::FLAG_H) or (regs.a() & 0x0F) > 9) {
            correction += 0x06;
        }
        if (carry_flag or regs.a() > 0x99) {
            correction += 0x60;
            carry_flag = true;
        }
        regs.a() += correction;
    } else { // After subtraction
        if (regs.get_flag(Registers::FLAG_H)) {
            correction += 0x06;
        }
        if (carry_flag) {
            correction += 0x60;
        }
        regs.a() -= correction;
        // Carry flag remains unchanged after subtraction
    }

    regs.set_flag(Registers::FLAG_Z, regs.a() == 0);
    regs.set_flag(Registers::FLAG_H, false);
    regs.set_flag(Registers::FLAG_C, carry_flag);
}

void CPU::op_cpl()
{
    regs.a() = ~regs.a();
    regs.set_flag(Registers::FLAG_N, true);
    regs.set_flag(Registers::FLAG_H, true);
}

void CPU::op_add_sp_r8()
{
    int8_t e = static_cast<int8_t>(fetch_byte());
    uint16_t result = regs.sp + e;
    regs.set_flag(Registers::FLAG_Z, false);
    regs.set_flag(Registers::FLAG_N, false);
    regs.set_flag(Registers::FLAG_H, ((regs.sp & 0xF) + (e & 0xF)) > 0xF);
    regs.set_flag(Registers::FLAG_C, ((regs.sp & 0xFF) + (e & 0xFF)) > 0xFF);
    regs.sp = result;
}

inline void CPU::update_rotate_flags(uint8_t result, bool c)
{
    regs.set_flag(Registers::FLAG_Z, result == 0x0);
    regs.set_flag(Registers::FLAG_N, false);
    regs.set_flag(Registers::FLAG_H, false);
    regs.set_flag(Registers::FLAG_C, c);
}

inline void CPU::op_rlc_reg8(uint8_t& reg)
{
    bool b7 = (reg >> 7) & 0x1;
    reg = (reg << 1) | b7;
    update_rotate_flags(reg, b7);
}

void CPU::op_rlc__hl_()
{
    uint8_t data = memory.read_byte(regs.hl.get());
    bool b7 = (data >> 7) & 0x1;
    uint8_t result = (data << 1) | b7;
    memory.write_byte(regs.hl.get(), result);
    update_rotate_flags(result, b7);
}

inline void CPU::op_rrc_reg8(uint8_t& reg)
{
    bool b0 = reg & 0x1;
    reg = (b0 << 7) | (reg >> 1);
    update_rotate_flags(reg, b0);
}

void CPU::op_rrc__hl_()
{
    uint8_t data = memory.read_byte(regs.hl.get());
    bool b0 = data & 0x1;
    uint8_t result = (b0 << 7) | (data >> 1);
    memory.write_byte(regs.hl.get(), result);
    update_rotate_flags(result, b0);
}

inline void CPU::op_rl_reg8(uint8_t& reg)
{
    bool b7 = (reg >> 7) & 0x1;
    reg = (reg << 1) | (regs.get_flag(Registers::FLAG_C));
    update_rotate_flags(reg, b7);
}

void CPU::op_rl__hl_()
{
    uint8_t data = memory.read_byte(regs.hl.get());
    bool b7 = (data >> 7) & 0x1;
    uint8_t result = (data << 1) | (regs.get_flag(Registers::FLAG_C));
    memory.write_byte(regs.hl.get(), result);
    update_rotate_flags(result, b7);
}

inline void CPU::op_rr_reg8(uint8_t& reg)
{
    bool b0 = reg & 0x1;
    reg = (regs.get_flag(Registers::FLAG_C) << 7) | (reg >> 1);
    update_rotate_flags(reg, b0);
}

void CPU::op_rr__hl_()
{
    uint8_t data = memory.read_byte(regs.hl.get());
    bool b0 = data & 0x1;
    uint8_t result = (regs.get_flag(Registers::FLAG_C) << 7) | (data >> 1);
    memory.write_byte(regs.hl.get(), result);
    update_rotate_flags(result, b0);
}

inline void CPU::op_sla_reg8(uint8_t& reg)
{
    bool b7 = (reg >> 7) & 0x1;
    reg = (reg << 1);
    update_rotate_flags(reg, b7);
}

void CPU::op_sla__hl_()
{
    uint8_t data = memory.read_byte(regs.hl.get());
    bool b7 = (data >> 7) & 0x1;
    uint8_t result = (data << 1);
    memory.write_byte(regs.hl.get(), result);
    update_rotate_flags(result, b7);
}

inline void CPU::op_sra_reg8(uint8_t& reg)
{
    bool b7 = (reg >> 7) & 0x1;
    bool b0 = reg & 0x1;
    reg = (reg >> 1) | (b7 << 7);
    update_rotate_flags(reg, b0);
}

void CPU::op_sra__hl_()
{
    uint8_t data = memory.read_byte(regs.hl.get());
    bool b7 = (data >> 7) & 0x1;
    bool b0 = data & 0x1;
    uint8_t result = (data >> 1) | (b7 << 7);
    memory.write_byte(regs.hl.get(), result);
    update_rotate_flags(result, b0);
}

inline void CPU::op_swap_reg8(uint8_t& reg)
{
    reg = (reg >> 4) | (reg << 4);
    update_rotate_flags(reg, false);
}

void CPU::op_swap__hl_()
{
    uint8_t data = memory.read_byte(regs.hl.get());
    uint8_t result = (data >> 4) | ((data << 4) & 0xf0);
    memory.write_byte(regs.hl.get(), result);
    update_rotate_flags(result, false);
}

inline void CPU::op_srl_reg8(uint8_t& reg)
{
    bool b0 = reg & 0x1;
    reg = (reg >> 1);
    update_rotate_flags(reg, b0);
}

void CPU::op_srl__hl_()
{
    uint8_t data = memory.read_byte(regs.hl.get());
    bool b0 = data & 0x1;
    uint8_t result = (data >> 1);
    memory.write_byte(regs.hl.get(), result);
    update_rotate_flags(result, b0);
}

inline void CPU::bit_b_reg8(const uint8_t reg, const uint8_t bit)
{
    regs.set_flag(Registers::FLAG_Z, ((reg >> bit) & 0x1) == 0x0);
    regs.set_flag(Registers::FLAG_N, false);
    regs.set_flag(Registers::FLAG_H, true);
}

void CPU::op_bit_b__hl_(const uint8_t bit)
{
    bit_b_reg8(memory.read_byte(regs.hl.get()), bit);
}

inline void CPU::res_b_reg8(uint8_t& reg, const uint8_t bit)
{
    reg &= ~(0x1 << bit);
}

void CPU::op_res_b__hl_(const uint8_t bit)
{
    memory.at(regs.hl.get()) &= ~(0x1 << bit);
}

inline void CPU::set_b_reg8(uint8_t& reg, const uint8_t bit)
{
    reg |= 0x1 << bit;
}
void CPU::op_set_b__hl_(const uint8_t bit)
{
    memory.at(regs.hl.get()) |= 0x1 << bit;
}
