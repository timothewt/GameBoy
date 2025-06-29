#include "gameboy.hpp"
#include <iostream>

int main(int argc, char* argv[])
{
    if (argc != 2) {
        std::cout << "Usage: " << argv[0] << " <ROM path>" << std::endl;
        throw std::runtime_error("ROM file not specified.");
    }
    GameBoy gameboy {};
    gameboy.load_rom(argv[1]);
    gameboy.run();
    return 0;
}
