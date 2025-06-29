#pragma once

#include <cstdint>

/**
 * @brief Represents a 16-bit register pair. Used to conveniently store the high and low byte.
 */
union RegisterPair {
    struct {
        uint8_t lo; /**< Lower byte (e.g., B). */
        uint8_t hi; /**< Higher byte (e.g., C). */
    };
    uint16_t val; /**< 16-bit register value (e.g., BC). */

    void set(uint16_t value)
    {
        val = value;
    }
};

/**
 * @brief Represents the CPU registers of the Game Boy.
 *
 * Contains the 8-bit register pairs AF, BC, DE, HL, and the stack pointer (SP) and program counter (PC). The flags
 * register is the low byte of AF.
 */
struct Registers {
    RegisterPair af, bc, de, hl; /**< CPU registers. */
    uint16_t pc; /**< Program counter pointing on the next instruction in memory. */
    uint16_t sp; /**< Stack pointer. */

    uint8_t& a() { return af.hi; };
    uint8_t& f() { return af.lo; };
    uint8_t& b() { return bc.hi; };
    uint8_t& c() { return bc.lo; };
    uint8_t& d() { return de.hi; };
    uint8_t& e() { return de.lo; };
    uint8_t& h() { return hl.hi; };
    uint8_t& l() { return hl.lo; };

    static constexpr uint8_t FLAG_Z = 1 << 7; /**< Zero flag. */
    static constexpr uint8_t FLAG_N = 1 << 6; /**< Subtract flag. */
    static constexpr uint8_t FLAG_H = 1 << 5; /**< Half Carry flag. */
    static constexpr uint8_t FLAG_C = 1 << 4; /**< Carry flag. */

    /**
     * @brief Retrieves the value of a given flag.
     *
     * @param flag Flag to retrieve the value of.
     * @return true if the flag is set to 1, else false.
     */
    bool get_flag(uint8_t flag) { return (f() & flag) != 0; }
    /**
     * @brief Sets the value of a flag
     *
     * @param flag Flag to set the value of.
     * @param val Either true of false. Value the flag will be set to.
     */
    void set_flag(uint8_t flag, bool val)
    {
        if (val)
            f() |= flag;
        else
            f() &= ~flag;
    }
};
