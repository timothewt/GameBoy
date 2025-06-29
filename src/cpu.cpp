#include "cpu.hpp"
#include "memory.hpp"
#include <format>

CPU::CPU(Memory& memory)
    : memory(memory)
{
    regs.af.set(0x01b0);
    regs.bc.set(0x0013);
    regs.de.set(0x00d8);
    regs.hl.set(0x014d);
    regs.pc = 0x0100;
    regs.sp = 0xfffe;

    setup_opcode_table();
}

void CPU::setup_opcode_table()
{
    opcode_table[0x00] = [this]() { op_nop(); };
}

void CPU::cycle()
{
    opcode = memory.read_byte(regs.pc++);
    decode_and_execute();
}

uint8_t CPU::fetch_byte()
{
    return memory.read_byte(regs.pc++);
}
uint16_t CPU::fetch_word()
{
    return fetch_byte() | (fetch_byte() << 8);
}

void CPU::decode_and_execute()
{
    if (opcode_table.contains(opcode))
        opcode_table[opcode]();
    else
        throw std::runtime_error(std::format("Unknown opcode: 0x{:X}", opcode));
}

void CPU::op_nop() { }
