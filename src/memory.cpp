#include "memory.hpp"
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

Memory::Memory()
{
    io_regs.fill(0x00); // Initialize all I/O registers to 0x00
    io_regs[0x00] = 0xCF; // P1
    io_regs[0x01] = 0x00; // SB: Serial Data
    io_regs[0x02] = 0x7E; // SC: Serial Control for DMG
    io_regs[0x07] = 0xF8; // TAC
    io_regs[0x44] = 0x90; // LY
    write_byte(0xFF05, 0x00); // TIMA
    write_byte(0xFF06, 0x00); // TMA
    write_byte(0xFF07, 0x00); // TAC
    write_byte(0xFF10, 0x80); // NR10
    write_byte(0xFF11, 0xBF); // NR11
    write_byte(0xFF12, 0xF3); // NR12
    write_byte(0xFF14, 0xBF); // NR14
    write_byte(0xFF16, 0x3F); // NR21
    write_byte(0xFF17, 0x00); // NR22
    write_byte(0xFF19, 0xBF); // NR24
    write_byte(0xFF1A, 0x7F); // NR30
    write_byte(0xFF1B, 0xFF); // NR31
    write_byte(0xFF1C, 0x9F); // NR32
    write_byte(0xFF1E, 0xBF); // NR33
    write_byte(0xFF20, 0xFF); // NR41
    write_byte(0xFF21, 0x00); // NR42
    write_byte(0xFF22, 0x00); // NR43
    write_byte(0xFF23, 0xBF); // NR30
    write_byte(0xFF24, 0x77); // NR50
    write_byte(0xFF25, 0xF3); // NR51
    write_byte(0xFF26, 0xF1); // NR52 (GB) or 0xF0 (SGB)
    write_byte(0xFF40, 0x91); // LCDC
    write_byte(0xFF42, 0x00); // SCY
    write_byte(0xFF43, 0x00); // SCX
    write_byte(0xFF45, 0x00); // LYC
    write_byte(0xFF47, 0xFC); // BGP
    write_byte(0xFF48, 0xFF); // OBP0
    write_byte(0xFF49, 0xFF); // OBP1
    write_byte(0xFF4A, 0x00); // WY
    write_byte(0xFF4B, 0x00); // WX
    write_byte(0xFFFF, 0x00); // IE

    interrupt_reg = 0x00; // IE: Interrupt Enable
}

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
    return at(address);
}

uint8_t& Memory::at(uint16_t address)
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
    else if (address == 0xffff)
        return interrupt_reg;
    return default_return;
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
