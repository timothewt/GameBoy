#pragma once

#include "cpu.hpp"
#include "memory.hpp"
#include <string>

/**
 * @brief Main Game Boy class managing the overall system.
 */
class GameBoy {
public:
    /**
     * @brief Game Boy class constructor.
     */
    GameBoy();
    /**
     * @brief Loads a ROM in memory.
     *
     * @param filename The Game Boy ROM file path (.gb).
     */
    void load_rom(const std::string& filename);
    /**
     * @brief Starts the program previously loaded into memory.
     */
    void run();

private:
    CPU cpu; /**< Game Boy CPU handling the execution of the operation codes read from the ROM memory. */
    Memory memory; /**< Game Boy memory, with the loaded ROM, RAM and so on. */
};
