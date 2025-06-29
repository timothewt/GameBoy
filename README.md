# Game Boy Emulator

Game Boy emulator in C++.

## How to use

1. Clone the repository
```
git clone https://github.com/timothewt/GameBoy.git
cd GameBoy
```

2. 
```
mkdir build
cd build
cmake ..
make
```

3. Start the program with one of the available ROMs
```
./gameboy ../roms/[rom].gb
```

## Dependencies

- SDL2

## License

MIT License

## References

- [Pan Docs](https://gbdev.io/pandocs/)
- [Game Boy: Complete Technical Reference](https://gekkio.fi/files/gb-docs/gbctr.pdf)
- [Gameboy (LR35902) OPCODES](https://www.pastraiser.com/cpu/gameboy/gameboy_opcodes.html)
