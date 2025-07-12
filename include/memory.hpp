#pragma once

#include <array>
#include <string>
#include <vector>

/**
 * @brief Checks if a value is in a given bounds (bounds included).
 *
 * This is used to choose which place of the memory to use.
 *
 * @param value Value to check.
 * @param a Lower bound.
 * @param b Higher bound.
 * @return true if the value is in [a, b], false otherwise.
 */
inline bool is_in_between(uint16_t value, uint16_t a, uint16_t b)
{
    return (a <= value) and (value <= b);
}

/**
 * @brief Memory class, storing the ROM, RAM an so on.
 */
class Memory {
public:
    /**
     * @brief Class constructor
     */
    Memory();

    static constexpr uint16_t IF_ADDR = 0xff0f;
    static constexpr uint16_t IE_ADDR = 0xffff;
    /**
     * @brief Loads a ROM into memory.
     *
     * @param filename Path of the Game Boy ROM file (.gb).
     * @throws std::runtime_error if the ROM file could not be opened.
     */
    void load_rom(const std::string& filename);
    /**
     * @brief Reads a byte from the memory at a given address.
     *
     * @param address Address of the byte to read.
     * @return The value of the byte at this address, given by copy.
     */
    uint8_t read_byte(uint16_t address);
    /**
     * @brief Reads a byte from the memory at a given address.
     *
     * @param address Address of the byte to read.
     * @return The value of the byte at this address, given by reference.
     */
    uint8_t& at(uint16_t address);
    /**
     * @brief Writes the value of a byte in the memory at a given address.
     *
     * @param address Address to write to.
     * @param value New value of the byte.
     */
    void write_byte(uint16_t address, uint8_t value);

private:
    std::vector<uint8_t> rom; /**< Cartridge ROM data, dynamically sized to the loaded game. */
    std::array<uint8_t, 0x2000> vram; /**< Video RAM, stores tile and background graphics. */
    std::array<uint8_t, 0x2000> ram; /**< External cartridge RAM, battery-backed in some cartridges. */
    std::array<uint8_t, 0x2000> wram; /**< Work RAM internal to the Game Boy. */
    std::array<uint8_t, 0xA0> oam; /**< Object Attribute Memory, stores sprite attributes. */
    std::array<uint8_t, 0x80> io_regs; /**< I/O Registers, hardware control and status. */
    std::array<uint8_t, 0x7f> hram; /**< High RAM, fast internal memory. */
    uint8_t interrupt_reg; /**< Interrupt Enable Register. */
    uint8_t default_return = 0xff; /**< Default return value for fetching. */
};
