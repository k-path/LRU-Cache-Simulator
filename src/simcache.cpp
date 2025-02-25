/*
CS-UY 2214
Kelvin Sapathy
simcache.cpp
*/

#include <cstddef>
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <iomanip>
#include <regex>
#include <cstdlib>

using namespace std;

// Some helpful constant values that we'll be using.
size_t const static NUM_REGS = 8;
size_t const static MEM_SIZE = 1 << 13;
size_t const static REG_SIZE = 1 << 16;

/*
    Loads an E20 machine code file into the list
    provided by mem. We assume that mem is
    large enough to hold the values in the machine
    code file.

    @param f Open file to read from
    @param mem Array represetnting memory into which to read program
*/
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

/*
    Prints the current state of the simulator, including
    the current program counter, the current register values,
    and the first memquantity elements of memory.

    @param pc The final value of the program counter
    @param regs Final value of all registers
    @param memory Final value of memory
    @param memquantity How many words of memory to dump
*/
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
// ^^^^^SIM.CPP FUNCTIONS^^^^^^^
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


/*
    Updates the bookkeeping vector by appending the just accessed value to the end of the vector and deleting that value from the vector if it already exists
    Call would usually be: update_bookkeeping(bookkeeping1 or 2[L1line or L2line], (L1blockID / L1numlines));

    @param bookkeeping_vec The bookkeeping vector itself.

    @param tag The just accessed tag.
    
*/
void update_bookkeeping(vector<int>& bookkeeping_vec, int tag) {
    if (find(bookkeeping_vec.begin(), bookkeeping_vec.end(), tag) != bookkeeping_vec.end()) { // if the number about to be pushed back is already in bookkeeping
        bookkeeping_vec.erase(find(bookkeeping_vec.begin(), bookkeeping_vec.end(), tag)); // erase first occurrence of the duplicate
        bookkeeping_vec.push_back(tag); // then append it 
    }
    else { // if the number about to be pushed back is not in bookkeeping already
        bookkeeping_vec.push_back(tag); // append it
    }
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

    int L1size;
    int L1assoc;
    int L1blocksize;
    int L1numlines;
    int L2size;
    int L2assoc;
    int L2blocksize;
    int L2numlines;
    bool two_caches = false;
    // moved these^ outside of starter code so it can be accessed later

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
            L1size = parts[0];
            L1assoc = parts[1];
            L1blocksize = parts[2];


            // DO: calculate L1numlines and print cache configuration
            L1numlines = L1size / (L1assoc * L1blocksize);
            print_cache_config("L1", L1size, L1assoc, L1blocksize, L1numlines);
        }
        else if (parts.size() == 6) {
            two_caches = true;
            L1size = parts[0];
            L1assoc = parts[1];
            L1blocksize = parts[2];
            L2size = parts[3];
            L2assoc = parts[4];
            L2blocksize = parts[5];
            // TODO: execute E20 program and simulate two caches here


            
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

    vector<int> a1_L1cache;  // L1cache with assoc of 1
    vector<vector<int>> a_n_L1cache; // L1cache with assoc of n

    vector<int> a1_L2cache;  // L2cache with assoc of 1
    vector<vector<int>> a_n_L2cache; // L2cache with assoc of 1
    // cache creation
    if (two_caches == false) { // only L1 cache
        if (L1assoc == 1) {
            a1_L1cache.resize(L1numlines, -1); // since this is direct mapped (assoc is 1) we dont need vec of a vec of ints, there will only be 1 tag for each line
        }
        else {
            vector<int> inner_vec(L1assoc, -1);
            a_n_L1cache.resize(L1numlines, inner_vec);               // each inner vector depends on the associativity, the outer vector depends on the number of lines
        }
    }
    else { // both L1 and L2 caches
        // Must create two caches
        if (L1assoc == 1) {
            a1_L1cache.resize(L1numlines, -1); // since this is direct mapped (assoc is 1) we dont need vec of a vec of ints, there will only be 1 tag for each line
        }
        else {
            vector<int> inner_vec(L1assoc, -1);
            a_n_L1cache.resize(L1numlines, inner_vec);               // each inner vector depends on the associativity, the outer vector depends on the number of lines
        }


        if (L2assoc == 1) {
            a1_L2cache.resize(L2numlines, -1);
        }
        else {
            vector<int> inner_vec(L2assoc, -1);
            a_n_L2cache.resize(L2numlines, inner_vec);
        }
    }
    
    vector<vector<int>> bookkeeping(L1numlines); // this vector keeps track of the order each of the tags for each line was accessed FOR L1cache
    vector<vector<int>> bookkeeping2(L2numlines); // same thing but for L2cache
    
    int LRU_int;  // this will refer to the first val of each line, so bookkeeping[L1line][0]
    vector<int>::iterator LRU_iter;  // the iter for the location of that element in the vector

    int LRU_int2;   // bookkeeping2[L2line][0]
    vector<int>::iterator LRU_iter2;  // it's iter for location in the vector

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
        else if (bits_extracter(memory[PC], 3, 13) == 4) { // its a LW *****************************************************************************************************

                
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

                int addr = registers[regAddr] + imm;

                int L1blockID = addr / L1blocksize;
                int L1line = L1blockID % L1numlines; // % (L1size / L1blocksize)

                int L2blockID = addr / L2blocksize;
                int L2line = L2blockID % L2numlines;

                

                if (two_caches == false) {
                    if (L1assoc == 1) { // if it just a direct mapped cache
                        
                        if (a1_L1cache[L1line] != (L1blockID / L1numlines)) { // if the new tag is not same as the tag currently in the cache, evict it with the new one. This will be a MISS
                            a1_L1cache[L1line] = L1blockID / L1numlines;
                            print_log_entry("L1", "MISS", PC, addr, L1line);
                        }
                        else {
                            print_log_entry("L1", "HIT", PC, addr, L1line);
                        }
                           
                    }
                    else { // not direct mapped
                        vector<int>::iterator location_neg1 = find(a_n_L1cache[L1line].begin(), a_n_L1cache[L1line].end(), -1);
                        vector<int>::iterator current_tag = find(a_n_L1cache[L1line].begin(), a_n_L1cache[L1line].end(), L1blockID / L1numlines);
                        

                        if (location_neg1 != a_n_L1cache[L1line].end()) { // if -1 is found in the vector
                            if (current_tag == a_n_L1cache[L1line].end()) { // if the current tag is not already in the vector add it at the -1 slot
                                
                                a_n_L1cache[L1line][(location_neg1)-(a_n_L1cache[L1line].begin())] = (L1blockID / L1numlines);
                                print_log_entry("L1", "MISS", PC, addr, L1line);
                                
                                update_bookkeeping(bookkeeping[L1line], (L1blockID / L1numlines));
                            }
                            else { // if the current tag is already in the cache vector, it's a HIT (so print) and push it to bookkeeping (since it was just accessed)
                                print_log_entry("L1", "HIT", PC, addr, L1line);
                                
                                update_bookkeeping(bookkeeping[L1line], (L1blockID / L1numlines));
                            }
                        }
                        else { // -1 is not found in vector so we must evict stuff
                            LRU_int = bookkeeping[L1line][0];
                            
                            LRU_iter = find(a_n_L1cache[L1line].begin(), a_n_L1cache[L1line].end(), LRU_int);

                            if (current_tag == a_n_L1cache[L1line].end()) { // if the current tag is not already in the vector put it at the LRU slot
                                
                                a_n_L1cache[L1line][LRU_iter - a_n_L1cache[L1line].begin()] = L1blockID / L1numlines; // replace the new tag to where the LRU tag was
                                bookkeeping[L1line].erase(find(bookkeeping[L1line].begin(), bookkeeping[L1line].end(), LRU_int)); //LRU tag is evicted so you need to remove it from bookkeeping because its no longer in the cache!
                                print_log_entry("L1", "MISS", PC, addr, L1line);
                                
                                // then update bookkeeping (append the "just accessed" tag to the bookkeeping vector)
                                update_bookkeeping(bookkeeping[L1line], (L1blockID / L1numlines));

                            }
                            else { // if the current tag is already in the cache vector, it's a HIT (so print) and push it to bookkeeping (since it was just accessed)
                                print_log_entry("L1", "HIT", PC, addr, L1line);
                                
                                // then update bookkeeping (append the "just accessed" tag to the bookkeeping vector)
                                update_bookkeeping(bookkeeping[L1line], (L1blockID / L1numlines));
                            }

            
                        }
                          // the a_n_cache (associativity of n >1) is represented as a vector of a vector of ints
                        // the outer vector represents each line, the outer vector size depends on the numlines. The inner vector represents each tag, the inner vector size depends on the associativty
                        // an associativity of 1 means we don't need an vec of vec of ints because each line will be limited to one tag
                        // here's how to pop a first element. vec.erase(vec.begin());

                    }

                }
                else { // two caches
                    if (L1assoc == 1) { // if it L1 just a direct mapped cache

                        if (a1_L1cache[L1line] != (L1blockID / L1numlines)) { // if the new tag is not same as the tag currently in the cache, evict it with the new one. This will be a MISS
                            a1_L1cache[L1line] = L1blockID / L1numlines;
                            print_log_entry("L1", "MISS", PC, addr, L1line);
                            if (L2assoc == 1) { // both L1 and L2 are direct mapped
                                if (a1_L2cache[L2line] != (L2blockID / L2numlines)) {
                                    a1_L2cache[L2line] = L2blockID / L2numlines;
                                    print_log_entry("L2", "MISS", PC, addr, L2line);
                                }
                                else {
                                    print_log_entry("L2", "HIT", PC, addr, L2line);
                                }
                            }
                            else { // L2 not direct mapped but L1 is direct mapped
                                vector<int>::iterator location_neg1_2 = find(a_n_L2cache[L2line].begin(), a_n_L2cache[L2line].end(), -1);
                                vector<int>::iterator current_tag_2 = find(a_n_L2cache[L2line].begin(), a_n_L2cache[L2line].end(), L2blockID / L2numlines);

                                if (location_neg1_2 != a_n_L2cache[L2line].end()) {
                                    if (current_tag_2 == a_n_L2cache[L2line].end()) {

                                        a_n_L2cache[L2line][(location_neg1_2)-(a_n_L2cache[L2line].begin())] = (L2blockID / L2numlines);
                                        print_log_entry("L2", "MISS", PC, addr, L2line);
                                        update_bookkeeping(bookkeeping2[L2line], (L2blockID / L2numlines));
                                    }
                                    else {
                                        print_log_entry("L2", "HIT", PC, addr, L2line);

                                        update_bookkeeping(bookkeeping2[L2line], (L2blockID / L2numlines));
                                    }
                                }
                                else {
                                    LRU_int2 = bookkeeping2[L2line][0];
                                    LRU_iter2 = find(a_n_L2cache[L2line].begin(), a_n_L2cache[L2line].end(), LRU_int2);

                                    if (current_tag_2 == a_n_L2cache[L2line].end()) { // if the current tag is not already in the vector put it at the LRU slot

                                        a_n_L2cache[L2line][LRU_iter2 - a_n_L2cache[L2line].begin()] = L2blockID / L2numlines; // replace the new tag to where the LRU tag was
                                        bookkeeping2[L2line].erase(find(bookkeeping2[L2line].begin(), bookkeeping2[L2line].end(), LRU_int2)); //LRU tag is evicted so you need to remove it from bookkeeping because its no longer in the cache!
                                        print_log_entry("L2", "MISS", PC, addr, L2line);

                                        // then update bookkeeping (append the "just accessed" tag to the bookkeeping vector)
                                        update_bookkeeping(bookkeeping2[L2line], (L2blockID / L2numlines));

                                    }
                                    else { // if the current tag is already in the cache vector, it's a HIT (so print) and push it to bookkeeping (since it was just accessed)
                                        print_log_entry("L2", "HIT", PC, addr, L2line);

                                        // then update bookkeeping (append the "just accessed" tag to the bookkeeping vector)
                                        update_bookkeeping(bookkeeping2[L2line], (L2blockID / L2numlines));
                                    }
                                }
                            }
                        }
                        else {
                            print_log_entry("L1", "HIT", PC, addr, L1line);
                        }

                    }
                    else { // L1 not direct mapped
                        vector<int>::iterator location_neg1 = find(a_n_L1cache[L1line].begin(), a_n_L1cache[L1line].end(), -1);
                        vector<int>::iterator current_tag = find(a_n_L1cache[L1line].begin(), a_n_L1cache[L1line].end(), L1blockID / L1numlines);

                        


                        if (location_neg1 != a_n_L1cache[L1line].end()) { // if -1 is found in the vector
                            if (current_tag == a_n_L1cache[L1line].end()) { // if the current tag is not already in the vector add it at the -1 slot
                                
                                a_n_L1cache[L1line][(location_neg1)-(a_n_L1cache[L1line].begin())] = (L1blockID / L1numlines);
                                print_log_entry("L1", "MISS", PC, addr, L1line);
                                update_bookkeeping(bookkeeping[L1line], (L1blockID / L1numlines));



                                // **********************************L2
                                if (L2assoc == 1) { // both L1 and L2 are direct mapped
                                    if (a1_L2cache[L2line] != (L2blockID / L2numlines)) {
                                        a1_L2cache[L2line] = L2blockID / L2numlines;
                                        print_log_entry("L2", "MISS", PC, addr, L2line);
                                    }
                                    else {
                                        print_log_entry("L2", "HIT", PC, addr, L2line);
                                    }
                                }
                                else { // L2 not direct mapped but L1 is direct mapped
                                    vector<int>::iterator location_neg1_2 = find(a_n_L2cache[L2line].begin(), a_n_L2cache[L2line].end(), -1);
                                    vector<int>::iterator current_tag_2 = find(a_n_L2cache[L2line].begin(), a_n_L2cache[L2line].end(), L2blockID / L2numlines);

                                    if (location_neg1_2 != a_n_L2cache[L2line].end()) {
                                        if (current_tag_2 == a_n_L2cache[L2line].end()) {

                                            a_n_L2cache[L2line][(location_neg1_2)-(a_n_L2cache[L2line].begin())] = (L2blockID / L2numlines);
                                            print_log_entry("L2", "MISS", PC, addr, L2line);
                                            update_bookkeeping(bookkeeping2[L2line], (L2blockID / L2numlines));
                                        }
                                        else {
                                            print_log_entry("L2", "HIT", PC, addr, L2line);

                                            update_bookkeeping(bookkeeping2[L2line], (L2blockID / L2numlines));
                                        }
                                    }
                                    else {
                                        LRU_int2 = bookkeeping2[L2line][0];
                                        LRU_iter2 = find(a_n_L2cache[L2line].begin(), a_n_L2cache[L2line].end(), LRU_int2);

                                        if (current_tag_2 == a_n_L2cache[L2line].end()) { // if the current tag is not already in the vector put it at the LRU slot

                                            a_n_L2cache[L2line][LRU_iter2 - a_n_L2cache[L2line].begin()] = L2blockID / L2numlines; // replace the new tag to where the LRU tag was
                                            bookkeeping2[L2line].erase(find(bookkeeping2[L2line].begin(), bookkeeping2[L2line].end(), LRU_int2)); //LRU tag is evicted so you need to remove it from bookkeeping because its no longer in the cache!
                                            print_log_entry("L2", "MISS", PC, addr, L2line);

                                            // then update bookkeeping (append the "just accessed" tag to the bookkeeping vector)
                                            update_bookkeeping(bookkeeping2[L2line], (L2blockID / L2numlines));

                                        }
                                        else { // if the current tag is already in the cache vector, it's a HIT (so print) and push it to bookkeeping (since it was just accessed)
                                            print_log_entry("L2", "HIT", PC, addr, L2line);

                                            // then update bookkeeping (append the "just accessed" tag to the bookkeeping vector)
                                            update_bookkeeping(bookkeeping2[L2line], (L2blockID / L2numlines));
                                        }
                                    }
                                }
                                // ************************L2


                            }
                            else { // if the current tag is already in the cache vector, it's a HIT (so print) and push it to bookkeeping (since it was just accessed)
                                print_log_entry("L1", "HIT", PC, addr, L1line);
                                
                                update_bookkeeping(bookkeeping[L1line], (L1blockID / L1numlines));
                            }
                        }
                        else { // -1 is not found in vector so we must evict stuff
                            LRU_int = bookkeeping[L1line][0];
                            LRU_iter = find(a_n_L1cache[L1line].begin(), a_n_L1cache[L1line].end(), LRU_int);

                            



                            if (current_tag == a_n_L1cache[L1line].end()) { // if the current tag is not already in the vector put it at the LRU slot
                                
                                a_n_L1cache[L1line][LRU_iter - a_n_L1cache[L1line].begin()] = L1blockID / L1numlines; // replace the new tag to where the LRU tag was
                                bookkeeping[L1line].erase(find(bookkeeping[L1line].begin(), bookkeeping[L1line].end(), LRU_int)); //LRU tag is evicted so you need to remove it from bookkeeping because its no longer in the cache!
                                print_log_entry("L1", "MISS", PC, addr, L1line);
                                update_bookkeeping(bookkeeping[L1line], (L1blockID / L1numlines));

                                //*********L2
                                if (L2assoc == 1) { // both L1 and L2 are direct mapped
                                    if (a1_L2cache[L2line] != (L2blockID / L2numlines)) {
                                        a1_L2cache[L2line] = L2blockID / L2numlines;
                                        print_log_entry("L2", "MISS", PC, addr, L2line);
                                    }
                                    else {
                                        print_log_entry("L2", "HIT", PC, addr, L2line);
                                    }
                                }
                                else { // L2 not direct mapped but L1 is direct mapped
                                    vector<int>::iterator location_neg1_2 = find(a_n_L2cache[L2line].begin(), a_n_L2cache[L2line].end(), -1);
                                    vector<int>::iterator current_tag_2 = find(a_n_L2cache[L2line].begin(), a_n_L2cache[L2line].end(), L2blockID / L2numlines);

                                    if (location_neg1_2 != a_n_L2cache[L2line].end()) {
                                        if (current_tag_2 == a_n_L2cache[L2line].end()) {

                                            a_n_L2cache[L2line][(location_neg1_2)-(a_n_L2cache[L2line].begin())] = (L2blockID / L2numlines);
                                            print_log_entry("L2", "MISS", PC, addr, L2line);
                                            update_bookkeeping(bookkeeping2[L2line], (L2blockID / L2numlines));
                                        }
                                        else {
                                            print_log_entry("L2", "HIT", PC, addr, L2line);

                                            update_bookkeeping(bookkeeping2[L2line], (L2blockID / L2numlines));
                                        }
                                    }
                                    else {
                                        LRU_int2 = bookkeeping2[L2line][0];
                                        LRU_iter2 = find(a_n_L2cache[L2line].begin(), a_n_L2cache[L2line].end(), LRU_int2);

                                        if (current_tag_2 == a_n_L2cache[L2line].end()) { // if the current tag is not already in the vector put it at the LRU slot

                                            a_n_L2cache[L2line][LRU_iter2 - a_n_L2cache[L2line].begin()] = L2blockID / L2numlines; // replace the new tag to where the LRU tag was
                                            bookkeeping2[L2line].erase(find(bookkeeping2[L2line].begin(), bookkeeping2[L2line].end(), LRU_int2)); //LRU tag is evicted so you need to remove it from bookkeeping because its no longer in the cache!
                                            print_log_entry("L2", "MISS", PC, addr, L2line);

                                            // then update bookkeeping (append the "just accessed" tag to the bookkeeping vector)
                                            update_bookkeeping(bookkeeping2[L2line], (L2blockID / L2numlines));

                                        }
                                        else { // if the current tag is already in the cache vector, it's a HIT (so print) and push it to bookkeeping (since it was just accessed)
                                            print_log_entry("L2", "HIT", PC, addr, L2line);

                                            // then update bookkeeping (append the "just accessed" tag to the bookkeeping vector)
                                            update_bookkeeping(bookkeeping2[L2line], (L2blockID / L2numlines));
                                        }
                                    }
                                }
                                //***************L2


                            }
                            else { // if the current tag is already in the cache vector, it's a HIT (so print) and push it to bookkeeping (since it was just accessed)
                                print_log_entry("L1", "HIT", PC, addr, L1line);
                                
                                // then update bookkeeping (append the "just accessed" tag to the bookkeeping vector)
                                update_bookkeeping(bookkeeping[L1line], (L1blockID / L1numlines));
                            }


                        }
                        

                    }
                } // two caches if statement



                PC = (PC + 1) & (MEM_SIZE - 1);

                // in the case PC is past 8192, make sure 3 most significant bits are ignored
        }
        else if (bits_extracter(memory[PC], 3, 13) == 5) { // its an SW **************************************************************************************
                regAddr = bits_extracter(memory[PC], 3, 10);
                regSrc = bits_extracter(memory[PC], 3, 7);
                imm = bits_extracter(memory[PC], 7, 0);

                if (bits_extracter(imm, 1, 6) == 1) { // imm val msb is 1, should be negative
                    imm = (((~imm) & 127) + 1) * -1;
                }

                memory[(registers[regAddr] + imm) & (MEM_SIZE - 1)] = registers[regSrc]; // memory pointer must be in 13 bit range
                int addr = registers[regAddr] + imm;
                int L1blockID = addr / L1blocksize;
                int L1line = L1blockID % L1numlines; 

                int L2blockID = addr / L2blocksize;
                int L2line = L2blockID % L2numlines;

                
                // Since code is repeated from LW's cache implementation, most of the comments are removed
                if (two_caches == false) {  // has only one L1 cache
                    if (L1assoc == 1) {
                        if (a1_L1cache[L1line] != (L1blockID / L1numlines)) { // if the new tag is not same as the tag currently in the cache, evict it with the new one. T
                            a1_L1cache[L1line] = L1blockID / L1numlines;
                            print_log_entry("L1", "SW", PC, addr, L1line);
                        }
                        else {
                            print_log_entry("L1", "SW", PC, addr, L1line);
                        }
                    }
                    else {
                        vector<int>::iterator location_neg1 = find(a_n_L1cache[L1line].begin(), a_n_L1cache[L1line].end(), -1);
                        vector<int>::iterator current_tag = find(a_n_L1cache[L1line].begin(), a_n_L1cache[L1line].end(), L1blockID / L1numlines);


                        if (location_neg1 != a_n_L1cache[L1line].end()) { // if -1 is found in the vector
                            if (current_tag == a_n_L1cache[L1line].end()) { // if the current tag is not already in the vector add it at the -1 slot
                                
                                a_n_L1cache[L1line][(location_neg1)-(a_n_L1cache[L1line].begin())] = (L1blockID / L1numlines);
                                print_log_entry("L1", "SW", PC, addr, L1line);
                                
                                update_bookkeeping(bookkeeping[L1line], (L1blockID / L1numlines));
                            }
                            else { // if the current tag is already in the cache vector, it's a HIT (so print) and push it to bookkeeping (since it was just accessed)
                                print_log_entry("L1", "SW", PC, addr, L1line);
                                
                                update_bookkeeping(bookkeeping[L1line], (L1blockID / L1numlines));
                            }
                        }
                        else { // -1 is not found in vector so we must evict stuff
                            LRU_int = bookkeeping[L1line][0];
                            
                            LRU_iter = find(a_n_L1cache[L1line].begin(), a_n_L1cache[L1line].end(), LRU_int);

                            if (current_tag == a_n_L1cache[L1line].end()) { // if the current tag is not already in the vector put it at the LRU slot
                                
                                a_n_L1cache[L1line][LRU_iter - a_n_L1cache[L1line].begin()] = L1blockID / L1numlines; // replace the new tag to where the LRU tag was
                                bookkeeping[L1line].erase(find(bookkeeping[L1line].begin(), bookkeeping[L1line].end(), LRU_int)); //LRU tag is evicted so you need to remove it from bookkeeping because its no longer in the cache!
                                print_log_entry("L1", "SW", PC, addr, L1line);
                                
                                // then update bookkeeping (append the "just accessed" tag to the bookkeeping vector)
                                update_bookkeeping(bookkeeping[L1line], (L1blockID / L1numlines));

                            }
                            else { // if the current tag is already in the cache vector, it's a HIT (so print) and push it to bookkeeping (since it was just accessed)
                                print_log_entry("L1", "SW", PC, addr, L1line);
                                
                                // then update bookkeeping (append the "just accessed" tag to the bookkeeping vector)
                                update_bookkeeping(bookkeeping[L1line], (L1blockID / L1numlines));
                            }


                        }
                    }
                    //print_log_entry("L1", "SW", PC, addr, L1line);
                    
                } 
                else {  // has both L1 and L2 caches

                    // LW's 2 CACHE CODE, but EVERY log entry is SW (instead of HIT OR MISS) vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
                    if (L1assoc == 1) { // if it L1 just a direct mapped cache

                        if (a1_L1cache[L1line] != (L1blockID / L1numlines)) { // if the new tag is not same as the tag currently in the cache, evict it with the new one. This will be a SW
                            a1_L1cache[L1line] = L1blockID / L1numlines;
                            print_log_entry("L1", "SW", PC, addr, L1line);
                            if (L2assoc == 1) { // both L1 and L2 are direct mapped
                                if (a1_L2cache[L2line] != (L2blockID / L2numlines)) {
                                    a1_L2cache[L2line] = L2blockID / L2numlines;
                                    print_log_entry("L2", "SW", PC, addr, L2line);
                                }
                                else {
                                    print_log_entry("L2", "SW", PC, addr, L2line);
                                }
                            }
                            else { // L2 not direct mapped but L1 is direct mapped
                                vector<int>::iterator location_neg1_2 = find(a_n_L2cache[L2line].begin(), a_n_L2cache[L2line].end(), -1);
                                vector<int>::iterator current_tag_2 = find(a_n_L2cache[L2line].begin(), a_n_L2cache[L2line].end(), L2blockID / L2numlines);

                                if (location_neg1_2 != a_n_L2cache[L2line].end()) {
                                    if (current_tag_2 == a_n_L2cache[L2line].end()) {

                                        a_n_L2cache[L2line][(location_neg1_2)-(a_n_L2cache[L2line].begin())] = (L2blockID / L2numlines);
                                        print_log_entry("L2", "SW", PC, addr, L2line);
                                        update_bookkeeping(bookkeeping2[L2line], (L2blockID / L2numlines));
                                    }
                                    else {
                                        print_log_entry("L2", "SW", PC, addr, L2line);

                                        update_bookkeeping(bookkeeping2[L2line], (L2blockID / L2numlines));
                                    }
                                }
                                else {
                                    LRU_int2 = bookkeeping2[L2line][0];
                                    LRU_iter2 = find(a_n_L2cache[L2line].begin(), a_n_L2cache[L2line].end(), LRU_int2);

                                    if (current_tag_2 == a_n_L2cache[L2line].end()) { // if the current tag is not already in the vector put it at the LRU slot

                                        a_n_L2cache[L2line][LRU_iter2 - a_n_L2cache[L2line].begin()] = L2blockID / L2numlines; // replace the new tag to where the LRU tag was
                                        bookkeeping2[L2line].erase(find(bookkeeping2[L2line].begin(), bookkeeping2[L2line].end(), LRU_int2)); //LRU tag is evicted so you need to remove it from bookkeeping because its no longer in the cache!
                                        print_log_entry("L2", "SW", PC, addr, L2line);

                                        // then update bookkeeping (append the "just accessed" tag to the bookkeeping vector)
                                        update_bookkeeping(bookkeeping2[L2line], (L2blockID / L2numlines));

                                    }
                                    else { // if the current tag is already in the cache vector, it's a HIT (so print) and push it to bookkeeping (since it was just accessed)
                                        print_log_entry("L2", "SW", PC, addr, L2line);

                                        // then update bookkeeping (append the "just accessed" tag to the bookkeeping vector)
                                        update_bookkeeping(bookkeeping2[L2line], (L2blockID / L2numlines));
                                    }
                                }
                            }
                        }
                        else {
                            print_log_entry("L1", "SW", PC, addr, L1line);
                            print_log_entry("L2", "SW", PC, addr, L2line);
                        }

                    }
                    else { // L1 not direct mapped
                        vector<int>::iterator location_neg1 = find(a_n_L1cache[L1line].begin(), a_n_L1cache[L1line].end(), -1);
                        vector<int>::iterator current_tag = find(a_n_L1cache[L1line].begin(), a_n_L1cache[L1line].end(), L1blockID / L1numlines);




                        if (location_neg1 != a_n_L1cache[L1line].end()) { // if -1 is found in the vector
                            if (current_tag == a_n_L1cache[L1line].end()) { // if the current tag is not already in the vector add it at the -1 slot

                                a_n_L1cache[L1line][(location_neg1)-(a_n_L1cache[L1line].begin())] = (L1blockID / L1numlines);
                                print_log_entry("L1", "SW", PC, addr, L1line);
                                update_bookkeeping(bookkeeping[L1line], (L1blockID / L1numlines));



                                // **********************************
                                if (L2assoc == 1) { // both L1 and L2 are direct mapped
                                    if (a1_L2cache[L2line] != (L2blockID / L2numlines)) {
                                        a1_L2cache[L2line] = L2blockID / L2numlines;
                                        print_log_entry("L2", "SW", PC, addr, L2line);
                                    }
                                    else {
                                        print_log_entry("L2", "SW", PC, addr, L2line);
                                    }
                                }
                                else { // L2 not direct mapped but L1 is direct mapped
                                    vector<int>::iterator location_neg1_2 = find(a_n_L2cache[L2line].begin(), a_n_L2cache[L2line].end(), -1);
                                    vector<int>::iterator current_tag_2 = find(a_n_L2cache[L2line].begin(), a_n_L2cache[L2line].end(), L2blockID / L2numlines);

                                    if (location_neg1_2 != a_n_L2cache[L2line].end()) {
                                        if (current_tag_2 == a_n_L2cache[L2line].end()) {

                                            a_n_L2cache[L2line][(location_neg1_2)-(a_n_L2cache[L2line].begin())] = (L2blockID / L2numlines);
                                            print_log_entry("L2", "SW", PC, addr, L2line);
                                            update_bookkeeping(bookkeeping2[L2line], (L2blockID / L2numlines));
                                        }
                                        else {
                                            print_log_entry("L2", "SW", PC, addr, L2line);

                                            update_bookkeeping(bookkeeping2[L2line], (L2blockID / L2numlines));
                                        }
                                    }
                                    else {
                                        LRU_int2 = bookkeeping2[L2line][0];
                                        LRU_iter2 = find(a_n_L2cache[L2line].begin(), a_n_L2cache[L2line].end(), LRU_int2);

                                        if (current_tag_2 == a_n_L2cache[L2line].end()) { // if the current tag is not already in the vector put it at the LRU slot

                                            a_n_L2cache[L2line][LRU_iter2 - a_n_L2cache[L2line].begin()] = L2blockID / L2numlines; // replace the new tag to where the LRU tag was
                                            bookkeeping2[L2line].erase(find(bookkeeping2[L2line].begin(), bookkeeping2[L2line].end(), LRU_int2)); //LRU tag is evicted so you need to remove it from bookkeeping because its no longer in the cache!
                                            print_log_entry("L2", "SW", PC, addr, L2line);

                                            // then update bookkeeping (append the "just accessed" tag to the bookkeeping vector)
                                            update_bookkeeping(bookkeeping2[L2line], (L2blockID / L2numlines));

                                        }
                                        else { // if the current tag is already in the cache vector, it's a SW (so print) and push it to bookkeeping (since it was just accessed)
                                            print_log_entry("L2", "SW", PC, addr, L2line);

                                            // then update bookkeeping (append the "just accessed" tag to the bookkeeping vector)
                                            update_bookkeeping(bookkeeping2[L2line], (L2blockID / L2numlines));
                                        }
                                    }
                                }
                                // ************************


                            }
                            else { // if the current tag is already in the cache vector, it's a SW (so print) and push it to bookkeeping (since it was just accessed)
                                print_log_entry("L1", "SW", PC, addr, L1line);

                                update_bookkeeping(bookkeeping[L1line], (L1blockID / L1numlines));
                            }
                        }
                        else { // -1 is not found in vector so we must evict stuff
                            LRU_int = bookkeeping[L1line][0];
                            LRU_iter = find(a_n_L1cache[L1line].begin(), a_n_L1cache[L1line].end(), LRU_int);





                            if (current_tag == a_n_L1cache[L1line].end()) { // if the current tag is not already in the vector put it at the LRU slot

                                a_n_L1cache[L1line][LRU_iter - a_n_L1cache[L1line].begin()] = L1blockID / L1numlines; // replace the new tag to where the LRU tag was
                                bookkeeping[L1line].erase(find(bookkeeping[L1line].begin(), bookkeeping[L1line].end(), LRU_int)); //LRU tag is evicted so you need to remove it from bookkeeping because its no longer in the cache!
                                print_log_entry("L1", "SW", PC, addr, L1line);
                                update_bookkeeping(bookkeeping[L1line], (L1blockID / L1numlines));

                                //*********
                                if (L2assoc == 1) { // both L1 and L2 are direct mapped
                                    if (a1_L2cache[L2line] != (L2blockID / L2numlines)) {
                                        a1_L2cache[L2line] = L2blockID / L2numlines;
                                        print_log_entry("L2", "SW", PC, addr, L2line);
                                    }
                                    else {
                                        print_log_entry("L2", "SW", PC, addr, L2line);
                                    }
                                }
                                else { // L2 not direct mapped but L1 is direct mapped
                                    vector<int>::iterator location_neg1_2 = find(a_n_L2cache[L2line].begin(), a_n_L2cache[L2line].end(), -1);
                                    vector<int>::iterator current_tag_2 = find(a_n_L2cache[L2line].begin(), a_n_L2cache[L2line].end(), L2blockID / L2numlines);

                                    if (location_neg1_2 != a_n_L2cache[L2line].end()) {
                                        if (current_tag_2 == a_n_L2cache[L2line].end()) {

                                            a_n_L2cache[L2line][(location_neg1_2)-(a_n_L2cache[L2line].begin())] = (L2blockID / L2numlines);
                                            print_log_entry("L2", "SW", PC, addr, L2line);
                                            update_bookkeeping(bookkeeping2[L2line], (L2blockID / L2numlines));
                                        }
                                        else {
                                            print_log_entry("L2", "SW", PC, addr, L2line);

                                            update_bookkeeping(bookkeeping2[L2line], (L2blockID / L2numlines));
                                        }
                                    }
                                    else {
                                        LRU_int2 = bookkeeping2[L2line][0];
                                        LRU_iter2 = find(a_n_L2cache[L2line].begin(), a_n_L2cache[L2line].end(), LRU_int2);

                                        if (current_tag_2 == a_n_L2cache[L2line].end()) { // if the current tag is not already in the vector put it at the LRU slot

                                            a_n_L2cache[L2line][LRU_iter2 - a_n_L2cache[L2line].begin()] = L2blockID / L2numlines; // replace the new tag to where the LRU tag was
                                            bookkeeping2[L2line].erase(find(bookkeeping2[L2line].begin(), bookkeeping2[L2line].end(), LRU_int2)); //LRU tag is evicted so you need to remove it from bookkeeping because its no longer in the cache!
                                            print_log_entry("L2", "SW", PC, addr, L2line);

                                            // then update bookkeeping (append the "just accessed" tag to the bookkeeping vector)
                                            update_bookkeeping(bookkeeping2[L2line], (L2blockID / L2numlines));

                                        }
                                        else { // if the current tag is already in the cache vector, it's a HIT (so print) and push it to bookkeeping (since it was just accessed)
                                            print_log_entry("L2", "SW", PC, addr, L2line);

                                            // then update bookkeeping (append the "just accessed" tag to the bookkeeping vector)
                                            update_bookkeeping(bookkeeping2[L2line], (L2blockID / L2numlines));
                                        }
                                    }
                                }
                                //***************


                            }
                            else { // if the current tag is already in the cache vector, it's a HIT (so print) and push it to bookkeeping (since it was just accessed)
                                print_log_entry("L1", "SW", PC, addr, L1line);

                                // then update bookkeeping (append the "just accessed" tag to the bookkeeping vector)
                                update_bookkeeping(bookkeeping[L1line], (L1blockID / L1numlines));
                            }
                            

                        }


                    }

                    // LW's TWO CACHE ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
                    
                } // two_caches if statement

                
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


    return 0;
}
//ra0Eequ6ucie6Jei0koh6phishohm9

