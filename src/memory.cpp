#include "memory.hpp"
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

void Memory::load_rom(const std::string& path)
{
    if (!std::filesystem::exists(path))
        throw std::runtime_error(std::string("ROM file does not exist:") + path);

    uintmax_t size = std::filesystem::file_size(path);
    std::ifstream file(path, std::ios::binary);
    rom.resize(size);
    file.read(reinterpret_cast<char*>(rom.data()), size);
}

uint8_t Memory::read_byte(uint16_t address)
{
    if (is_in_between(address, 0x0, 0x7fff))
        return rom[address];
    else if (is_in_between(address, 0x8000, 0x9fff))
        return vram[address - 0x8000];
    else if (is_in_between(address, 0xa000, 0xbfff))
        return ram[address - 0xa000];
    else if (is_in_between(address, 0xc000, 0xdfff))
        return wram[address - 0xc000];
    else if (is_in_between(address, 0xe000, 0xfdff))
        return wram[address - 0xe000];
    else if (is_in_between(address, 0xfe00, 0xfe9f))
        return oam[address - 0xfe00];
    else if (is_in_between(address, 0xff00, 0xff7f))
        return io_regs[address - 0xff00];
    else if (is_in_between(address, 0xff80, 0xfffe))
        return hram[address - 0xff80];
    else if (address == 0xFFFF)
        return interrupt_reg;
    return 0xFF;
}

void Memory::write_byte(uint16_t address, uint8_t value)
{
    if (is_in_between(address, 0x8000, 0x9fff))
        vram[address - 0x8000] = value;
    else if (is_in_between(address, 0xa000, 0xbfff))
        ram[address - 0xa000] = value;
    else if (is_in_between(address, 0xc000, 0xdfff))
        wram[address - 0xc000] = value;
    else if (is_in_between(address, 0xe000, 0xfdff))
        wram[address - 0xe000] = value;
    else if (is_in_between(address, 0xfe00, 0xfe9f))
        oam[address - 0xfe00] = value;
    else if (is_in_between(address, 0xff00, 0xff7f))
        io_regs[address - 0xff00] = value;
    else if (is_in_between(address, 0xff80, 0xfffe))
        hram[address - 0xff80] = value;
    else if (address == 0xFFFF)
        interrupt_reg = value;
}
