/*
CS-UY 2214
Jeff Epstein
Starter code for E20 cache assembler
simcache.cpp
*/

#include <cstddef>
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <limits>
#include <iomanip>
#include <regex>
#include <cstdlib>

using namespace std;


size_t const static NUM_REGS = 8;
size_t const static MEM_SIZE = 1 << 13;
size_t const static REG_SIZE = 1 << 16;


void load_machine_code(ifstream& f, unsigned mem[]) {
    regex machine_code_re("^ram\\[(\\d+)\\] = 16'b(\\d+);.*$");
    size_t expectedaddr = 0;
    string line;
    while (getline(f, line)) {
        smatch sm;
        if (!regex_match(line, sm, machine_code_re)) {
            cerr << "Can't parse line: " << line << endl;
            exit(1);
        }
        size_t addr = stoi(sm[1], nullptr, 10);
        unsigned instr = stoi(sm[2], nullptr, 2);
        if (addr != expectedaddr) {
            cerr << "Memory addresses encountered out of sequence: " << addr << endl;
            exit(1);
        }
        if (addr >= MEM_SIZE) {
            cerr << "Program too big for memory" << endl;
            exit(1);
        }
        expectedaddr++;
        mem[addr] = instr;
    }
}

void print_state(unsigned pc, unsigned regs[], unsigned memory[], size_t memquantity) {
    cout << setfill(' ');
    cout << "Final state:" << endl;
    cout << "\tpc=" << setw(5) << pc << endl;

    for (size_t reg = 0; reg < NUM_REGS; reg++)
        cout << "\t$" << reg << "=" << setw(5) << regs[reg] << endl;

    cout << setfill('0');
    bool cr = false;
    for (size_t count = 0; count < memquantity; count++) {
        cout << hex << setw(4) << memory[count] << " ";
        cr = true;
        if (count % 8 == 7) {
            cout << endl;
            cr = false;
        }
    }

    if (cr)
        cout << endl;
}

unsigned bits_extracter(unsigned extractee, unsigned bit_length,
    unsigned position) {
    unsigned mask = ((1 << bit_length) - 1) << position;
    unsigned extraction = (extractee & mask) >> position;

    return extraction;
}


bool check_if_halt(unsigned opCode, unsigned PC) {
    // halt if it's a jump to itself
    // The halt instruction is translated by the assembler as an unconditional jump (j) to the current memory
    // location.
    //So first we need to check if the instruction type is unconditional jump, it can also be a halt if you use jr and that registers value is equal to the current PC
    bool is_halt = false;
    if (bits_extracter(opCode, 3, 13) == 2) {
        if (bits_extracter(opCode, 13, 0) == PC) { // if the immediate val is equal to current PC
            is_halt = true;
        }
        else {
            is_halt = false;
        }
    }

    return is_halt;
}

/*
    Prints out the correctly-formatted configuration of a cache.

    @param cache_name The name of the cache. "L1" or "L2"

    @param size The total size of the cache, measured in memory cells.
        Excludes metadata

    @param assoc The associativity of the cache. One of [1,2,4,8,16]

    @param blocksize The blocksize of the cache. One of [1,2,4,8,16,32,64])
*/
void print_cache_config(const string& cache_name, int size, int assoc, int blocksize, int num_lines) {
    cout << "Cache " << cache_name << " has size " << size <<
        ", associativity " << assoc << ", blocksize " << blocksize <<
        ", lines " << num_lines << endl;
}

/*
    Prints out a correctly-formatted log entry.

    @param cache_name The name of the cache where the event
        occurred. "L1" or "L2"

    @param status The kind of cache event. "SW", "HIT", or
        "MISS"

    @param pc The program counter of the memory
        access instruction

    @param addr The memory address being accessed.

    @param line The cache line or set number where the data
        is stored.
*/
void print_log_entry(const string& cache_name, const string& status, int pc, int addr, int line) {
    cout << left << setw(8) << cache_name + " " + status << right <<
        " pc:" << setw(5) << pc <<
        "\taddr:" << setw(5) << addr <<
        "\tline:" << setw(4) << line << endl;
}

/**
    Main function
    Takes command-line args as documented below
*/
int main(int argc, char* argv[]) {
    /*
        Parse the command-line arguments
    */
    char* filename = nullptr;
    bool do_help = false;
    bool arg_error = false;
    string cache_config;
    for (int i = 1; i < argc; i++) {
        string arg(argv[i]);
        if (arg.rfind("-", 0) == 0) {
            if (arg == "-h" || arg == "--help")
                do_help = true;
            else if (arg == "--cache") {
                i++;
                if (i >= argc)
                    arg_error = true;
                else
                    cache_config = argv[i];
            }
            else
                arg_error = true;
        }
        else {
            if (filename == nullptr)
                filename = argv[i];
            else
                arg_error = true;
        }
    }
    /* Display error message if appropriate */
    if (arg_error || do_help || filename == nullptr) {
        cerr << "usage " << argv[0] << " [-h] [--cache CACHE] filename" << endl << endl;
        cerr << "Simulate E20 cache" << endl << endl;
        cerr << "positional arguments:" << endl;
        cerr << "  filename    The file containing machine code, typically with .bin suffix" << endl << endl;
        cerr << "optional arguments:" << endl;
        cerr << "  -h, --help  show this help message and exit" << endl;
        cerr << "  --cache CACHE  Cache configuration: size,associativity,blocksize (for one" << endl;
        cerr << "                 cache) or" << endl;
        cerr << "                 size,associativity,blocksize,size,associativity,blocksize" << endl;
        cerr << "                 (for two caches)" << endl;
        return 1;
    }

    // sim.cpp main comes here
    ifstream f(filename);
    if (!f.is_open()) {
        cerr << "Can't open file " << filename << endl;
        return 1;
    }

    unsigned* memory = new unsigned[MEM_SIZE];
    unsigned* registers = new unsigned[NUM_REGS];
    // initialize all registers to 0
    for (size_t reg = 0; reg < NUM_REGS; reg++) {
        registers[reg] = 0;
    }

    load_machine_code(f, memory);


    unsigned PC = 0;
    unsigned regSrc = 0;
    unsigned regDst = 0;
    unsigned imm = 0;
    unsigned regA = 0;
    unsigned regB = 0;
    int rel_imm = 0;
    unsigned regAddr = 0;


    // TODO: your code here. print the final state of the simulator before ending, using print_state
    //print_state(PC, registers, memory, 128);
    //**If I call print_state it makes the register values come out in hex**



    // BEGIN CODE FOR CACHE HERE. 

    /* parse cache config */
    if (cache_config.size() > 0) {
        vector<int> parts;
        size_t pos;
        size_t lastpos = 0;
        while ((pos = cache_config.find(",", lastpos)) != string::npos) {
            parts.push_back(stoi(cache_config.substr(lastpos, pos)));
            lastpos = pos + 1;
        }
        parts.push_back(stoi(cache_config.substr(lastpos)));
        if (parts.size() == 3) { // that means only one cache (L1)
            int L1size = parts[0];
            int L1assoc = parts[1];
            int L1blocksize = parts[2];


            // DO: calculate L1numlines and print cache configuration
            int L1numlines;
            L1numlines = L1size / (L1assoc * L1blocksize);
            print_cache_config("L1", L1size, L1assoc, L1blocksize, L1numlines);

            // TODO: execute E20 program and simulate one cache here


            while (check_if_halt(memory[PC], PC) == false) {
                if (bits_extracter(memory[PC], 3, 13) == 1) { // its an addi/movi
                    // since its addi, lets extract regSrc, regDst, and imm

                    regSrc = bits_extracter(memory[PC], 3, 10);  // extracting which register is in regSrc position of addi
                    regDst = bits_extracter(memory[PC], 3, 7);   // extracting which register is in regDst position of addi 
                    imm = bits_extracter(memory[PC], 7, 0);      // extracting immediate value


                    if (bits_extracter(imm, 1, 6) == 1) { // imm val msb is 1, should be negative
                        imm = (((~imm) & 127) + 1) * -1;  // converts value to negative
                    }

                    if (regDst != 0) { // we can never allow the program to update register 0
                        registers[regDst] = (registers[regSrc] + imm) & (REG_SIZE - 1);  // "& (REG_SIZE - 1)" ensures we stay in 16 bit size range
                    }

                    // increment PC
                    PC = (PC + 1) & (MEM_SIZE - 1);   // doing & (MEM_SIZE-1) ensures it stays in 13 bit range
                }
                else if (bits_extracter(memory[PC], 3, 13) == 7) { // its an slti
                    regSrc = bits_extracter(memory[PC], 3, 10);
                    regDst = bits_extracter(memory[PC], 3, 7);
                    imm = bits_extracter(memory[PC], 7, 0);

                    if (regDst != 0) {
                        if ((registers[regSrc] & (REG_SIZE - 1)) < (imm & (REG_SIZE - 1))) { // we need to have the imm be 16 bits (same bits as the reg val) 
                            registers[regDst] = 1;
                        }
                        else {
                            registers[regDst] = 0;
                        }
                    }
                    PC = (PC + 1) & (MEM_SIZE - 1);
                }
                else if (bits_extracter(memory[PC], 3, 13) == 6) { // its an jeq
                    //unsigned imm = 0;
                    regA = bits_extracter(memory[PC], 3, 10);
                    regB = bits_extracter(memory[PC], 3, 7);
                    rel_imm = bits_extracter(memory[PC], 7, 0); // extracts rel_imm

                    //cout << "msb is  " << bits_extracter(rel_imm, 1, 6) << endl;
                    if (bits_extracter(rel_imm, 1, 6) == 1) { // jeq also has signed rel_imm so we need to convert to negative if msb is 1
                        rel_imm = (((~rel_imm) & 127) + 1) * -1;
                    }

                    if (registers[regA] == registers[regB]) {
                        PC = (PC + 1 + rel_imm) & (MEM_SIZE - 1);  // when we jump too we need to make sure program counter stays in range
                    }
                    else {
                        PC = (PC + 1) & (MEM_SIZE - 1);
                    }

                }
                else if (bits_extracter(memory[PC], 3, 13) == 0) { // it's a 3 reg arg instruction (add,sub,and..)
                    regA = bits_extracter(memory[PC], 3, 10);
                    regB = bits_extracter(memory[PC], 3, 7);
                    regDst = bits_extracter(memory[PC], 3, 4);
                    if (bits_extracter(memory[PC], 4, 0) == 0) { // it's an add
                        if (regDst != 0) {
                            registers[regDst] = (registers[regA] + registers[regB]) & (REG_SIZE - 1);
                            // & (REG_SIZE - 1) ensures the number stays in the 16 bit range
                        }
                        PC = (PC + 1) & (MEM_SIZE - 1);
                    }
                    else if (bits_extracter(memory[PC], 4, 0) == 1) { // it's a sub
                        if (regDst != 0) {
                            registers[regDst] = (registers[regA] - registers[regB]) & (REG_SIZE - 1);
                        }
                        PC = (PC + 1) & (MEM_SIZE - 1);
                    }
                    else if (bits_extracter(memory[PC], 4, 0) == 2) { // its an and
                        if (regDst != 0) {
                            registers[regDst] = (registers[regA] & registers[regB]) & (REG_SIZE - 1);
                        }
                        PC = (PC + 1) & (MEM_SIZE - 1);
                    }
                    else if (bits_extracter(memory[PC], 4, 0) == 3) { // its an or
                        if (regDst != 0) {
                            registers[regDst] = (registers[regA] | registers[regB]) & (REG_SIZE - 1);
                        }
                        PC = (PC + 1) & (MEM_SIZE - 1);
                    }
                    else if (bits_extracter(memory[PC], 4, 0) == 4) { // its a slt
                        if (regDst != 0) {
                            if (((registers[regA]) & (REG_SIZE - 1)) < (registers[regB] & (REG_SIZE - 1))) { // if a negative number is inside a register, interpret it as unsigned in 16 bit range
                                registers[regDst] = 1;
                            }
                            else {
                                registers[regDst] = 0;
                            }
                        }
                        PC = (PC + 1) & (MEM_SIZE - 1);
                    }
                    else if (bits_extracter(memory[PC], 4, 0) == 8) { // its a jr
                        regSrc = bits_extracter(memory[PC], 3, 10);
                        PC = (registers[regSrc]) & (MEM_SIZE - 1); // for the case where a val inside a register is more than 13 bits, ignore 3 most signif bits
                    }
                }
                else if (bits_extracter(memory[PC], 3, 13) == 4) { // its a LW ***************************************************
                    regAddr = bits_extracter(memory[PC], 3, 10);
                    regDst = bits_extracter(memory[PC], 3, 7);
                    imm = bits_extracter(memory[PC], 7, 0);

                    if (bits_extracter(imm, 1, 6) == 1) { // imm val msb is 1, should be negative
                        imm = (((~imm) & 127) + 1) * -1;
                    }

                    if (regDst != 0) {
                        registers[regDst] = (memory[(registers[regAddr] + imm) & (MEM_SIZE - 1)]) & (REG_SIZE - 1);
                        // makes sure sum of regAddr and imm val stays in range of memory size
                    }

                    PC = (PC + 1) & (MEM_SIZE - 1);
                    // in the case PC is past 8192, make sure 3 most significant bits are ignored
                }
                else if (bits_extracter(memory[PC], 3, 13) == 5) { // its an SW ***************************************************
                    regAddr = bits_extracter(memory[PC], 3, 10);
                    regSrc = bits_extracter(memory[PC], 3, 7);
                    imm = bits_extracter(memory[PC], 7, 0);

                    if (bits_extracter(imm, 1, 6) == 1) { // imm val msb is 1, should be negative
                        imm = (((~imm) & 127) + 1) * -1;
                    }

                    memory[(registers[regAddr] + imm) & (MEM_SIZE - 1)] = registers[regSrc]; // memory pointer must be in 13 bit range

                    // we'll do the cache stuff right before we increment the PC. 
                    // We need to figure how to get the address of the memory being stored. Nvm, the address of the memory is just the imm plus regAddr
                    int addr = registers[regAddr] + imm;


                    // if it is direct mapped cache. Line = address % numberoflines
                    /*clarify these things :
                     - if associativity is 1 then it is a direct mapped cache? Yes
                     - to find number of lines on a direct mapped cache you can do cachesize / blocksize
                    */

                    if (L1assoc < 2) {
                        int line = addr % (L1size / L1blocksize);
                        print_log_entry("L1", "SW", PC, addr, line);
                    }







                    PC = (PC + 1) & (MEM_SIZE - 1);
                }
                else if (bits_extracter(memory[PC], 3, 13) == 2) { // its a j
                    if (check_if_halt(memory[PC], PC) == false) {
                        PC = (bits_extracter(memory[PC], 13, 0)) & (MEM_SIZE - 1);
                    }
                    else {
                        PC = (PC + 1) & (MEM_SIZE - 1); // When its a halt, increment PC and then stop simulation. Keeps PC in approriate bit range
                        break; // end simulation (while loop)
                    }
                }
                else if (bits_extracter(memory[PC], 3, 13) == 3) { // its a jal
                    imm = bits_extracter(memory[PC], 13, 0);

                    registers[7] = PC + 1;

                    PC = imm & (MEM_SIZE - 1);
                }



            }






        }
        else if (parts.size() == 6) {
            int L1size = parts[0];
            int L1assoc = parts[1];
            int L1blocksize = parts[2];
            int L2size = parts[3];
            int L2assoc = parts[4];
            int L2blocksize = parts[5];
            // TODO: execute E20 program and simulate two caches here


            int L1numlines;
            int L2numlines;
            if (L1assoc < 2) { // if associativity is 1. This means it's direct mapped
                L1numlines = L1size / L1blocksize;
            }
            else { // if associativity is greater than 1, but the associativity * blocksize is equal to the cache size, then it's fully associative
                L1numlines = L1size / (L1assoc * L1blocksize);
            }

            if (L2assoc < 2) { // if associativity is 1. This means it's direct mapped
                L2numlines = L2size / L2blocksize;
            }
            else { // if associativity is greater than 1, but the associativity * blocksize is equal to the cache size, then it's fully associative
                L2numlines = L2size / (L2assoc * L2blocksize);
            }
            print_cache_config("L1", L1size, L1assoc, L1blocksize, L1numlines);
            print_cache_config("L2", L2size, L2assoc, L2blocksize, L2numlines);


        }
        else {
            cerr << "Invalid cache config" << endl;
            return 1;
        }
    }


    return 0;
}
//ra0Eequ6ucie6Jei0koh6phishohm9