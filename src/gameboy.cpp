#include "gameboy.hpp"
#include <string>

GameBoy::GameBoy()
    : memory()
    , cpu(memory)
    , ppu()
{
}

void GameBoy::load_rom(const std::string& filename)
{
    memory.load_rom(filename);
}

void GameBoy::run()
{
    while (true) {
        cpu.cycle();
        ppu.cycle();
    }
}
