#include <iostream>
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>
#include <iostream>
#include <fstream>
// #include <cstdint>
#include <cstring>
#include <chrono>

using namespace std;
/* CHIP-8 specs:
DMA/memory: 4kb
Display: 64x32 (x2 for super-chip) X
program counter PC (points at current instruction in mem): 12-bit (4095)
index register I (point at location in mem): 16-bit
stack for 16-bit addresses X
8-bit delay timer: dec at 60hz until 0
8-bit sound timer: same but beep aslong as !=0
16x 8-bit general-purpose variable registers (16th = flag register)

000 -> 1FF here emulator
200 -> end chip8 rom

font size 4w 5h

// 60hz 1000/60 -> 16ms delay
// 1 Mhz = 1000 Khz = 1000 000 Hz 1000/1000 000 -> 0.001 ms delay per instr = 1 microsec = 1000 nano
// 2 Mhz = 2000 Khz = 2000 000 Hz 1000/2000 000 -> 0.0005 ms delay per instr = 0.5 microsec = 500 nano
// 3 Mhz = 3000 Khz = 3000 000 Hz 1000/3000 000 -> 0.00033333 ms delay per instr = 0.3 microsec = 333 nano
// 4 Mhz = 4000 Khz = 4000 000 Hz 1000/4000 000 -> 0.00025 ms delay per instr = 0.25 microsec = 250 nano
*/

class DelayTimer
{
public:
    std::chrono::_V2::system_clock::time_point delay_timer_start_time = std::chrono::high_resolution_clock::now();

    bool done(bool time_accuracy_mode, int delay_amount)
    {
        auto delay_timer_finish_time = std::chrono::high_resolution_clock::now();
        if (time_accuracy_mode)
        {
            if (delay_amount <= std::chrono::duration_cast<std::chrono::milliseconds>(delay_timer_finish_time - delay_timer_start_time).count())
            {
                delay_timer_start_time = std::chrono::high_resolution_clock::now();
                return 1;
            }
        }
        else
        {
            if (delay_amount <= std::chrono::duration_cast<std::chrono::nanoseconds>(delay_timer_finish_time - delay_timer_start_time).count())
            {
                delay_timer_start_time = std::chrono::high_resolution_clock::now();
                return 1;
            }
        }
        return 0;
    }
};

void update_display(bool display_arr[64][32])
{

    for (size_t i = 0; i < 32; i++)
    {
        for (size_t i2 = 0; i2 < 64; i2++)
        {
            if (display_arr[i2][i])
            {
                cout << "##";
            }
            else
            {
                cout << "  ";
            }
        }
        cout << "\n";
    }
    cout << "\e[32A";
}

int main(int argc, char **argv)
{
    // init important variables
    vector<uint16_t> stack = {};

    bool display[64][32] = {};
    bool empty_display[64][32] = {};

    uint8_t mem[4096] = {};
    uint16_t PC = 0x200;
    uint16_t I;
    uint8_t delay_timer;
    uint8_t sound_timer;
    uint8_t GPVR[16] = {};

    // init font and dump it into DMA
    uint8_t font[80] = {
        // 4 wide 5 high
        0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
        0x20, 0x60, 0x20, 0x20, 0x70, // 1
        0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
        0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
        0x90, 0x90, 0xF0, 0x10, 0x10, // 4
        0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
        0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
        0xF0, 0x10, 0x20, 0x40, 0x40, // 7
        0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
        0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
        0xF0, 0x90, 0xF0, 0x90, 0x90, // A
        0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
        0xF0, 0x80, 0x80, 0x80, 0xF0, // C
        0xE0, 0x90, 0x90, 0x90, 0xE0, // D
        0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
        0xF0, 0x80, 0xF0, 0x80, 0x80  // F
    };

    copy(font, font + 80, mem + 0x050);

    // open rom_file and dump at 0x200
    ifstream rom_file;
    rom_file.open(argv[1], ios::binary | ios::ate); // ios::blahblah have constructers built in so the (argv, ios::) could have been slapped after ifstream rom_file

    if (!rom_file.is_open())
    {
        cout << "specified rom not found" << endl;
        return 0;
    }

    streampos size;

    size = rom_file.tellg();

    uint8_t *tmp_rom_array = new uint8_t[size];
    char *memblock = new char[size];

    rom_file.seekg(0, ios::beg);
    rom_file.read(memblock, size);
    memcpy(tmp_rom_array, memblock, size);

    copy(tmp_rom_array, tmp_rom_array + size, mem + 0x200);

    rom_file.close();

    // get cpu_delay_time
    int cpu_nanos_delay = 1000;
    if (argc >= 3)
    {
        cpu_nanos_delay /= atoi(argv[2]);
    }

    /*for (size_t i = 0; i < 4096; i++)
    {
        cout << to_string(mem[i]);
    }*/

    DelayTimer delay_timer_timer;
    DelayTimer cpu_delay_timer;

    uint8_t OP_nibble_X;
    uint8_t OP_nibble_Y;

    uint16_t fetched_instr = 0x000;

    // main loop
    while (true)
    {
        if (delay_timer_timer.done(1, 16))
        {
            delay_timer--;
            sound_timer--;
        }
        if (cpu_delay_timer.done(0, cpu_nanos_delay))
        {
            //  fetch
            // instruction type
            // look up 1/16 GPVR (X)                   ]
            // look up 1/16 GPVR (Y)]                  ]   12-bit mem adr
            // 4-bit int (N)        ]  8-bit int (NNN) ]

            fetched_instr = 256U * mem[PC] + mem[PC + 1];

            OP_nibble_X = (fetched_instr & 0x0F00) >> 8;
            OP_nibble_Y = (fetched_instr & 0x00F0) >> 4;

            //   decode and execute
            switch ((fetched_instr & 0xF000) >> 12)
            {
            case 0x0:
                switch (fetched_instr & 0x0FFF)
                {
                case 0x0E0: // clear display
                    memcpy(display, empty_display, 64 * 32);
                    update_display(display);
                    break;
                case 0x0EE: // rtrn from sub routine (got there via call inst)
                    PC = stack.back();
                    stack.pop_back();
                }
                break;
            case 0x1: // jump instr
                PC = fetched_instr & 0x0FFF;
                PC -= 2;
                break;
            case 0x2: // call instr (jmp but returns)
                stack.push_back(PC);
                PC = fetched_instr & 0x0FFF;
                PC -= 2;
                break;
            case 0x6:
                GPVR[OP_nibble_X] = fetched_instr & 0x00FF;
                break;
            case 0x7:
                GPVR[OP_nibble_X] += fetched_instr & 0x00FF;
                break;
            case 0xA:
                I = fetched_instr & 0x0FFF;
                break;
            case 0xD: // draw instr
                GPVR[15] = 0;
                for (size_t i = 0; i < ((fetched_instr & 0x000F)); i++)
                {
                    for (size_t i2 = 0; i2 < 8; i2++)
                    {
                        if (((GPVR[OP_nibble_Y] % 31) + i) < 32)
                        {
                            if (((GPVR[OP_nibble_X] % 63) + 8 - i2) < 64)
                            {
                                display[(GPVR[OP_nibble_X] % 63) + 8 - i2][(GPVR[OP_nibble_Y] % 31) + i] ^= ((mem[I + i] >> i2) & 0x1);
                                if ((display[(GPVR[OP_nibble_X] % 63) + 8 - i2][(GPVR[OP_nibble_Y] % 31) + i] == 1) && ((mem[I + i] >> i2) & 0x1) == 1)
                                {
                                    GPVR[15] = 1;
                                }
                            }
                        }
                    }
                }
                update_display(display);

                break;
            default:
                break;
            }
            PC += 2;
            if (PC >= 0xFFF)
            {
                PC = 0x200;
            }
        }
    }
    return 0;
}