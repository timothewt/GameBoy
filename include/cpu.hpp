#pragma once

#include "memory.hpp"
#include "registers.hpp"
#include <cstdint>
#include <functional>
#include <unordered_map>

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
    /**< Maps standard opcodes (0x00–0xFF) to their instructions */
    std::unordered_map<uint8_t, std::function<void()>> opcode_table {};
    /**< Maps CB-prefixed opcodes (0xCB00–0xCBFF) to their instructions */
    std::unordered_map<uint8_t, std::function<void()>> cbcode_table {};

    /**
     * @brief Sets up the operation code table to map an instruction to the correct method.
     */
    void setup_opcode_table();
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
     * CPU instructions.
     */
    void op_nop();
};
