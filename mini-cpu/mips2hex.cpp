#include <iostream>
#include <algorithm>
#include <string>
#include <vector>
#include <utility>
#include <map>
#include <fstream>

using namespace std;

enum InstructionTypes {
    R, S, I, ILS, J
};

/**
 * @brief There are 6 registers. This maps register $names to their address. Extract the last 4 bits.
 */
map<string, uint16_t> reg2addr{
    {"$zero", 0x0000},
    {"$t0", 0x0001},
    {"$t1", 0x0002},
    {"$t2", 0x0003},
    {"$t3", 0x0004},
    {"$t4", 0x0005},
    {"$sp", 0x0006}
};

/**
 * @brief Assign STACK_SEGMENT to $sp as the first instruction assembled, by default.
 */
const unsigned STACK_SEGMENT = 0x00;

/**
 * @brief There are 16 operations. This maps the instructions to their opcodes. Extract the last 4 bits.
 */
map<string, pair<uint16_t, InstructionTypes>> instr2op{
    {"addi", {0x0000, I}},
    {"subi", {0x1000, I}},
    {"and", {0x2000, R}},
    {"andi", {0x3000, I}},
    {"j", {0x4000, J}},
    {"sll", {0x5000, S}},
    {"bneq", {0x6000, I}},
    {"lw", {0x7000, ILS}},
    {"ori", {0x8000, I}},
    {"sub", {0x9000, R}},
    {"beq", {0xA000, I}},
    {"srl", {0xB000, S}},
    {"nor", {0xC000, R}},
    {"add", {0xD000, R}},
    {"or", {0xE000, R}},
    {"sw", {0xF000, ILS}}
};

/**
 * @brief Shifting amount to take LSB 4 bits to instruction format position.
 */
const unsigned SHIFT_RS = 8, SHIFT_RT = 4, SHIFT_RD = 0;
/**
 * @brief Shifting amount to take LSB 8 bits to instruction formal position.
 */
const unsigned SHIFT_JA = 4;

vector<uint16_t> convert_push_pop(string, string);
uint16_t convert_mips_to_hexcode(string, string);
uint16_t convert_Rtype(string, string);
uint16_t convert_Stype(string, string);
uint16_t convert_Itype(string, string);
uint16_t convert_Itype_loadstore(string, string);
uint16_t convert_Jtype(string, string);

vector<string> split_str(string, string);
void replace_substr(string&, string, string);

int main(int argc, char* argv[]) {
    if (argc != 2) {
        cerr << "Error: Assembler needs file to assemble" << endl;
        return 1;
    }

    ifstream mipsfile(argv[1]);
    if (!mipsfile.is_open()) {
        cerr << "Error: Could not open file: " << argv[0] << endl;
        return 1;
    }

    ofstream hexfile("hex.dat", ios::out | ios::binary);
    if (!hexfile.is_open()) {
        cerr << "Error: Could not open write file: hex.dat" << endl;
        return 1;
    }

    string mips;
    while (getline(mipsfile, mips)) {
        try {
            // pre process instruction
            replace_substr(mips, ", ", ",");
            vector<string> mips_split1 = split_str(mips, " ");
            string instruction = mips_split1[0], operands = mips_split1[1];

            if (instruction == "push" || instruction == "pop") {
                vector<uint16_t> hexcodes = convert_push_pop(instruction, operands);
                hexfile.write(reinterpret_cast<const char*>(&hexcodes[0]), sizeof(uint16_t));
                hexfile.write(reinterpret_cast<const char*>(&hexcodes[1]), sizeof(uint16_t));
            } else {
                uint16_t hexcode = convert_mips_to_hexcode(instruction, operands);
                hexfile.write(reinterpret_cast<const char*>(&hexcode), sizeof(uint16_t));
                // cout << hexcode << endl; // Bytes are stored in little-endian format (LSByte first)
            }
        } catch (int exception) {
            cerr << "Could not convert instruction" << endl;
        }
    }

    mipsfile.close();
    hexfile.close();

    return 0;
}

uint16_t convert_mips_to_hexcode(string instruction, string operands) {
    if (instr2op.count(instruction) == 0) {
        throw 1;
    } else if (instr2op[instruction].second == R) {
        return convert_Rtype(instruction, operands);
    } else if (instr2op[instruction].second == S) {
        return convert_Stype(instruction, operands);
    } else if (instr2op[instruction].second == I) {
        return convert_Itype(instruction, operands);
    } else if (instr2op[instruction].second == ILS) {
        return convert_Itype_loadstore(instruction, operands);
    } else {
        return convert_Jtype(instruction, operands);
    }
}

// Converts push pop to 2 instructions.
vector<uint16_t> convert_push_pop(string operation, string operands) {
    vector<uint16_t> hexcodes;
    if (operation == "push") {
        hexcodes.push_back(convert_Itype_loadstore("sw", operands + "," + "0($sp)"));
        hexcodes.push_back(convert_Itype("addi", "$sp,$sp,-1"));
    } else if (operation == "pop") {
        hexcodes.push_back(convert_Itype_loadstore("lw", operands + "," + "0($sp)"));
        hexcodes.push_back(convert_Itype("addi", "$sp,$sp,1"));
    }
    return hexcodes;
}

uint16_t convert_Rtype(string operation, string operands) {
    vector<string> args = split_str(operands, ",");
    string rd = args[0], rs = args[1], rt = args[2];

    // cout << rs << " " << rt << " " << rd << endl;
    uint16_t hexcode = 0;
    hexcode |= instr2op[operation].first;
    hexcode |= (reg2addr[rs] << SHIFT_RS);
    hexcode |= (reg2addr[rt] << SHIFT_RT);
    hexcode |= (reg2addr[rd] << SHIFT_RD);

    return hexcode;
}

uint16_t convert_Stype(string operation, string operands) {
    vector<string> args = split_str(operands, ",");
    string rt = args[0], rs = args[1];
    uint16_t shamt_x16 = stoi(args[2]);

    // cout << rs << " " << rt << " " << shamt << endl;
    uint16_t hexcode = 0;
    hexcode |= instr2op[operation].first;
    hexcode |= (reg2addr[rs] << SHIFT_RS);
    hexcode |= (reg2addr[rt] << SHIFT_RT);
    hexcode |= (shamt_x16 << SHIFT_RD);

    return hexcode;
}

uint16_t convert_Itype(string operation, string operands) {
    vector<string> args = split_str(operands, ",");
    string rt = args[0], rs = args[1];
    int16_t addr_imm_x16 = stoi(args[2]);
    if (addr_imm_x16 < 0) {
        addr_imm_x16 &= 0x000F; // zero-ing every bits other than 4 LSB bits (immi range -8, 7)
    }

    // cout << rs << " " << rt << " " << addr_imm << endl;
    uint16_t hexcode = 0;
    hexcode |= instr2op[operation].first;
    hexcode |= (reg2addr[rs] << SHIFT_RS);
    hexcode |= (reg2addr[rt] << SHIFT_RT);
    hexcode |= (addr_imm_x16 << SHIFT_RD);

    return hexcode;
}

uint16_t convert_Itype_loadstore(string operation, string operands) {
    vector<string> args0 = split_str(operands, ",");
    string rt = args0[0];
    vector<string> args1 = split_str(args0[1], "(");
    args1[1].pop_back(); // removes trailing )
    string offset = args1[0], rs = args1[1];
    int16_t offset_x16 = stoi(offset);
    if (offset_x16 < 0) {
        offset_x16 &= 0x000F; // zero-ing every bits other than 4 LSB bits (offset range -8, 7)
    }

    // cout << rs << " " << rt << " " << offset << endl;
    uint16_t hexcode = 0;
    hexcode |= instr2op[operation].first;
    hexcode |= (reg2addr[rs] << SHIFT_RS);
    hexcode |= (reg2addr[rt] << SHIFT_RT);
    hexcode |= (offset_x16 << SHIFT_RD);

    return hexcode;
}

uint16_t convert_Jtype(string operation, string operands) {
    uint16_t addr_x16 = stoi(operands);

    // cout << operands << endl;
    uint16_t hexcode = 0;
    hexcode |= instr2op[operation].first;
    hexcode |= (addr_x16 << SHIFT_JA);
    return hexcode;
}

vector<string> split_str(string str, string delim) {
    vector<string> split;
    int str_start = 0;
    for (int delim_pos = str.find(delim); delim_pos != string::npos; delim_pos = str.find(delim, str_start)) {
        split.push_back(str.substr(str_start, delim_pos - str_start));
        str_start = delim_pos + delim.length();
    }
    split.push_back(str.substr(str_start));
    return split;
}

void replace_substr(string& subject, string target, string replacement) {
    size_t pos = 0;
    while ((pos = subject.find(target, pos)) != string::npos) {
        subject.replace(pos, target.length(), replacement);
        pos += replacement.length();
    }
}