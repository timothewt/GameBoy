#pragma once

#include "memory.hpp"
#include "registers.hpp"
#include <cstdint>
#include <functional>
#include <unordered_map>

/**
 * @brief Retrieves the Most Significant Byte of a 2-bytes word.
 *
 * @param word Word to retrieve the MSB from.
 * @return The MSB of the word.
 */
inline uint8_t msb(uint16_t word)
{
    return word >> 8;
}

/**
 * @brief Retrieves the Least Significant Byte of a 2-bytes word.
 *
 * @param word Word to retrieve the LSB from.
 * @return The LSB of the word.
 */
inline uint8_t lsb(uint16_t word)
{
    return word & 0xFF;
}

/**
 * @brief Builds a 16-bit (2 bytes) word from a less Significant bit and a most significant bit.
 *
 * @param lsb Least Significant Bit
 * @param msb Most Significant Bit
 * @return The word made from the lsb and msb.
 */
inline uint16_t build_word(uint8_t lsb, uint8_t msb)
{
    return static_cast<uint16_t>(lsb) | (static_cast<uint16_t>(msb) << 8);
}

/**
 * @brief CPU class, fetching, decoding and executing operations from the memory.
 */
class CPU {
public:
    /**
     * @brief Constructor of the Game Boy.
     */
    CPU(Memory& memory);
    /**
     * @brief Makes a fetch->decode->execute cycle.
     */
    void cycle();

private:
    Memory& memory; /**< Reference to the Game Boy memory. */
    Registers regs {}; /**< CPU Registers. */
    uint16_t opcode {}; /**< Current operation code read from the memory. */
    uint8_t cycles_left {}; /**< Number of cycles left for the previous instruction. */
    uint64_t total_cycles {}; /** < Total number of elapsed cycles. */
    /**< Maps standard opcodes (0x00–0xFF) to their instructions */
    std::unordered_map<uint8_t, std::function<void()>> opcode_table {};
    /**< Maps CB-prefixed opcodes (0xCB00–0xCBFF) to their instructions */
    std::unordered_map<uint8_t, std::function<void()>> cbcode_table {};
    /**< Maps an operation code to its corresponding number of cycles. */
    std::unordered_map<uint8_t, uint8_t> instruction_cycles {};
    /**< Maps a CB-prefixed operation code to its corresponding number of cycles. */
    std::unordered_map<uint8_t, uint8_t> cb_instruction_cycles {};

    bool stopped { false }; /**< true if the CPU has been stopped by the STOP instructions. */
    bool halted { false }; /**< true if the CPU has been halted by the HALT instructions. */
    bool halt_bug { false }; /**< true if the HALT bug occurs (i.e.,  IME = 0 and [IE] & [IF] != 0). */
    bool ime { false }; /**< Interrupt Master Enable flag. */
    bool ime_next { false }; /**< Whether to put IME to true after the next instruction. */

    /**
     * @brief Checks the IE and IF values in the memory to check if an interrupt is pending.
     *
     * @return true if an interrupt is pending false otherwise.
     */
    bool interrupt_pending();
    /**
     * @brief Handles all operations regarding the interrupts.
     */
    void handle_interrupts();
    /**
     * @brief Handles all operations regarding the timers.
     */
    void handle_timers();
    /**
     * @brief Outputs the content of the serial output in the console.
     */
    void check_serial_output();
    /**
     * @brief Sets up the operatieon code table and the instruction cycles table to map an instruction to the correct
     * method and the number of cycles.
     */
    void setup_tables();
    /**
     * @brief Used to efficiently register the 8-bit registers manipulation operations.
     *
     * The operations registered with it are ADD, ADC, SUB, SBC, AND, XOR, OR, CP.
     *
     * @param start_code The start code of the instruction group.
     * @param reg8_getters The function used to retrieve the 8-bit register by reference.
     * @param fn The function to call on the register.
     */
    void register_reg8_manip_group(uint8_t start_code,
        const std::array<std::function<uint8_t&()>, 8>& reg8_getters,
        std::function<void(uint8_t&)> fn);
    /**
     * @brief Used to efficiently register the first section of the 0xCB operations.
     *
     * The operations registered with it are RLC, RRC, RL, RR, SLA, SRA, SWAP, SRL.
     *
     * @param start_code The start code of the instruction group.
     * @param reg8_getters The function used to retrieve the 8-bit register by reference.
     * @param reg8_fn The function to call on the 8-bit registers.
     * @param hl_fn The function to call on the memory byte located at the address HL.
     */
    void register_cb_group(uint8_t start_code,
        const std::array<std::function<uint8_t&()>, 8>& reg8_getters,
        std::function<void(uint8_t&)> reg8_fn,
        std::function<void()> hl_fn);
    /**
     * @brief Used to efficiently register the second section of the 0xCB operations.
     *
     * The operations registered with it are BIT, RES, SET.
     *
     * @param start_code The start code of the instruction group.
     * @param reg8_getters The function used to retrieve the 8-bit register by reference.
     * @param reg8_fn The function to call on the 8-bit registers.
     * @param hl_fn The function to call on the memory byte located at the address HL.
     */
    template <typename Reg8Fn>
    void register_cb_group_large(uint8_t start_code,
        const std::array<std::function<uint8_t&()>, 8>& reg8_getters,
        Reg8Fn reg8_fn,
        std::function<void(const uint8_t)> hl_fn);
    /**
     * @brief Fetches the next byte from memory pointed by the program counter (PC) and increments PC.
     *
     * @return The next byte (8 bits) at the current PC.
     */
    uint8_t fetch_byte();
    /**
     * @brief Fetches the next two bytes (16 bits) from memory in little-endian order and increments PC accordingly.
     *
     * @return The combined 16-bit value of the next two bytes.
     */
    uint16_t fetch_word();
    /**
     * @brief Decodes and execute the current instruction.
     */
    void decode_and_execute();

    /**
     * CPU instructions. See https://gekkio.fi/files/gb-docs/gbctr.pdf..
     */
    void op_nop();
    void op_stop();
    void op_halt();
    void op_di();
    void op_ei();

    /**
     * @brief Increases the value of a 16-bit register reg by one.
     *
     * @param reg The 16-bit register to increment.
     */
    inline void inc_reg(RegisterPair& reg);
    void op_inc_sp();
    /**
     * @brief Increases the value of an 8-bit register reg by one.
     *
     * @param reg The 8-bit register to decrement.
     */
    inline void inc_reg(uint8_t& reg);
    /**
     * @brief Gives the result of the incrementation of a register value, and updates the flags accordingly.
     *
     * @param reg_val The current value of the register.
     * @return The incremented register value.
     */
    inline uint8_t inc_reg8_update_flags(uint8_t reg_val);
    void op_inc__hl_();

    /**
     * @brief Decreases the value of a 16-bit register reg by one.
     *
     * @param reg The 16-bit register to decrement.
     */
    inline void dec_reg(RegisterPair& reg);
    void op_dec_sp();
    /**
     * @brief Decreases the value of an 8-bit register reg by one.
     *
     * @param reg The 8-bit register to decrement.
     */
    inline void dec_reg(uint8_t& reg);
    /**
     * @brief Gives the result of the decrementation of a register value, and updates the flags accordingly.
     *
     * @param reg_val The current value of the register.
     * @return The decremented register value.
     */
    inline uint8_t dec_reg8_update_flags(uint8_t reg_val);
    void op_dec__hl_();

    /**
     * @brief Adds the value of a 16-bit register into the HL register and updates the flags accordingly.
     *
     * @param value Value of the register to add to HL.
     */
    inline void add_hl_reg16(uint16_t value);

    /**
     * @brief jumps to the given address (i.e., sets the PC to this address).
     *
     * @param addr The new address of the PC.
     */
    inline void jp_to(uint16_t addr);
    /**
     * @brief jumps to the given address (i.e., sets the PC to this address) if the condition is met. Also updates
     * the cycles_left in this case.
     *
     * @param addr The new address of the PC.
     * @param condition The condition under which the jump is made.
     */
    inline void conditional_jp_to(uint16_t addr, bool condition);
    void op_jp_a16();
    void op_jp__hl_();
    void op_jp_nz_a16();
    void op_jp_nc_a16();
    void op_jp_z_a16();
    void op_jp_c_a16();

    /**
     * @brief Jumps relatively according to the given offset.
     *
     * @param unsigned_offset The unsigned offset that the address will jump of. Will be casted to signed.
     */
    inline void jr_of(uint8_t unsigned_offset);
    /**
     * @brief Jumps relatively according to the given offset if the condition is met. Updates the cycles_left if it is
     * the case.
     *
     * @param unsigned_offset The unsigned offset that the address will jump of. Will be casted to signed.
     * @param condition Condition under which the jump will be made.
     */
    inline void conditional_jr_of(uint8_t unsigned_offset, bool condition);
    void op_jr_r8();
    void op_jr_nz_r8();
    void op_jr_nc_r8();
    void op_jr_z_r8();
    void op_jr_c_r8();

    /**
     * @brief Calls (jumps to an address and keeps in the stack the current position) a function from the memory.
     *
     * @param addr Address of the function to call.
     */
    inline void call_to(uint16_t addr);
    /**
     * @brief Calls (jumps to an address and keeps in the stack the current position) a function from the memory if the
     * condition is met.
     *
     * @param addr Address of the function to call.
     * @param condition Condition under which the function call is made.
     */
    inline void conditional_call_to(uint16_t addr, bool condition);
    void op_call_a16();
    void op_call_nz_a16();
    void op_call_nc_a16();
    void op_call_z_a16();
    void op_call_c_a16();

    void op_ret();
    void op_reti();
    /**
     * @brief Returns to the last address saved in the stack if a condition is met.
     *
     * @param condition The condition under which the return is done.
     */
    inline void conditional_ret(bool condition);
    void op_ret_nz();
    void op_ret_nc();
    void op_ret_z();
    void op_ret_c();

    void op_ld_bc_d16();
    void op_ld_de_d16();
    void op_ld_hl_d16();
    void op_ld_sp_d16();
    void op_ld__a16__sp();
    void op_ld_hl_sp_r8();
    void op_ld_sp_hl();

    /**
     * @brief Loads the immediate 8-bit value in the memory into an 8-bit register.
     *
     * @param reg Register to load the value into.
     */
    inline void ld_reg8_d8(uint8_t& reg);
    void op_ld__hl__d8();

    /**
     * @brief Loads the value of an 8-bit register into another 8-bit register.
     *
     * @param dst Register into which the value will be loaded.
     * @param value Value of the source register, new value of dst.
     */
    inline void ld_r8_r8(uint8_t& dst, const uint8_t value);
    /**
     * @brief Loads the value of an 8-bit register into the memory at the address HL.
     *
     * @param value Value of the source register, new value of the byte at address HL.
     */
    inline void ld__hl__r8(const uint8_t value);

    void op_ld__bc__a();
    void op_ld__de__a();
    void op_ld__hlp__a();
    void op_ld__hlm__a();

    void op_ld_a__bc_();
    void op_ld_a__de_();
    void op_ld_a__hlp_();
    void op_ld_a__hlm_();

    void op_ld__a16__a();
    void op_ld_a__a16_();
    void op_ldh__a8__a();
    void op_ldh_a__a8_();
    void op_ld__c__a();
    void op_ld_a__c_();

    /**
     * @brief Pushes the value of a 16-bit register into the stack.
     *
     * @param reg Register which value is retrieved.
     */
    inline void push_reg(RegisterPair& reg);
    /**
     * @brief Pops the value of the stack into a 16-bit register.
     *
     * @param reg Register to store into.
     * @param clear_lower_4bits Whether to clear the lower 4 bits of the register after loading. Used for AF.
     */
    inline void pop_reg(RegisterPair& reg, bool clear_lower_4bits = false);

    /**
     * @brief Adds the value of an 8-bit register into the accumulator (register A). Updates the needed flags.
     *
     * @param reg_val Value of the 8-bit register to add to the accumulator.
     * @param use_carry Whether to use the carry or not in the addition.
     */
    inline void add_reg_update_flags(uint8_t reg_val, bool use_carry = false);
    void op_add_d8();
    void op_adc_d8();
    /**
     * @brief Subtracts the value of an 8-bit register from the accumulator (register A). Updates the needed flags.
     *
     * @param reg_val Value of the 8-bit register to subtract from the accumulator.
     * @param use_carry Whether to use the carry or not in the addition.
     */
    inline void sub_reg_update_flags(uint8_t reg_val, bool use_carry = false);
    void op_sub_d8();
    void op_sbc_d8();

    /**
     * @brief Sets the accumulator to a and reg8.
     *
     * @param reg_val Value of the register used for the and operation.
     */
    inline void and_reg_update_flags(uint8_t reg_val);
    void op_and_d8();
    /**
     * @brief Sets the accumulator to a xor reg8.
     *
     * @param reg_val Value of the register used for the xor operation.
     */
    inline void xor_reg_update_flags(uint8_t reg_val);
    void op_xor_d8();
    /**
     * @brief Sets the accumulator to a or reg8.
     *
     * @param reg_val Value of the register used for the or operation.
     */
    inline void or_reg_update_flags(uint8_t reg_val);
    void op_or_d8();
    /**
     * @brief Sets the accumulator to a and reg8.
     *
     * @param reg_val Value of the register used for the and operation.
     */
    inline void cp_reg_update_flags(uint8_t reg_val);
    void op_cp_d8();

    /**
     * @brief Updates the flags after the rotation of the A register.
     *
     * @param c New value of the C flag.
     */
    inline void update_rotate_flags(bool c);
    void op_rlca();
    void op_rla();
    void op_rrca();
    void op_rra();

    void op_ccf();
    void op_scf();
    void op_daa();
    void op_cpl();

    void op_add_sp_r8();

    /**
     * @brief Updates the flags after the rotation an 8-bit register.
     *
     * @param result Result of the rotation.
     * @param c New value of the C flag.
     */
    inline void update_rotate_flags(const uint8_t result, const bool c);
    /**
     * @brief Rotate an 8-bit register to the left and sets the last bit to the previously first bit.
     *
     * @param reg 8-bit register to rotate.
     */
    inline void op_rlc_reg8(uint8_t& reg);
    void op_rlc__hl_();
    /**
     * @brief Rotate an 8-bit register to the right and sets the last bit to the previously first bit.
     *
     * @param reg 8-bit register to rotate.
     */
    inline void op_rrc_reg8(uint8_t& reg);
    void op_rrc__hl_();
    /**
     * @brief Rotate an 8-bit register to the left and sets the last bit to the C flag value.
     *
     * @param reg 8-bit register to rotate.
     */
    inline void op_rl_reg8(uint8_t& reg);
    void op_rl__hl_();
    /**
     * @brief Rotate an 8-bit register to the right and sets the last bit to the C flag value.
     *
     * @param reg 8-bit register to rotate.
     */
    inline void op_rr_reg8(uint8_t& reg);
    void op_rr__hl_();
    /**
     * @brief Shifts an 8-bit register to the left.
     *
     * @param reg 8-bit register to shift.
     */
    inline void op_sla_reg8(uint8_t& reg);
    void op_sla__hl_();
    /**
     * @brief Arithmetically shifts an 8-bit register to the right.
     *
     * @param reg 8-bit register to shift.
     */
    inline void op_sra_reg8(uint8_t& reg);
    void op_sra__hl_();
    /**
     * @brief Swaps the 4 lower bits of the register with the 4 higher bits.
     *
     * @param reg 8-bit register to swaps bits of.
     */
    inline void op_swap_reg8(uint8_t& reg);
    void op_swap__hl_();
    /**
     * @brief Logically shifts an 8-bit register to the right.
     *
     * @param reg 8-bit register to shift.
     */
    inline void op_srl_reg8(uint8_t& reg);
    void op_srl__hl_();
    /**
     * @brief Tests the bit of a register, setting the Z flag to true if the bit is 0.
     *
     * @param reg Register to test.
     * @param bit Bit to test.
     */
    inline void bit_b_reg8(const uint8_t reg, const uint8_t bit);
    void op_bit_b__hl_(const uint8_t bit);
    /**
     * @brief Resets the bit of a register.
     *
     * @param reg Register to operate on.
     * @param bit Bit to reset.
     */
    inline void res_b_reg8(uint8_t& reg, const uint8_t bit);
    void op_res_b__hl_(const uint8_t bit);
    /**
     * @brief Sets the bit of a register.
     *
     * @param reg Register to operate on.
     * @param bit Bit to set.
     */
    inline void set_b_reg8(uint8_t& reg, const uint8_t bit);
    void op_set_b__hl_(const uint8_t bit);
};
