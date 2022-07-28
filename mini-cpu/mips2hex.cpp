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
    {"$t4", 0x0005}
};

/**
 * @brief There are 16 operations. This maps the instructions to their opcodes. Extract the last 4 bits.
 */
map<string, pair<uint16_t, InstructionTypes>> instr2op{
    {"addi", {0x0000, I}},
    {"subi", {0x0001, I}},
    {"and", {0x0002, R}},
    {"andi", {0x0003, I}},
    {"j", {0x0004, J}},
    {"sll", {0x0005, S}},
    {"bneq", {0x0006, I}},
    {"lw", {0x0007, ILS}},
    {"ori", {0x0008, I}},
    {"sub", {0x0009, R}},
    {"beq", {0x000A, I}},
    {"srl", {0x000B, S}},
    {"nor", {0x000C, R}},
    {"add", {0x000D, R}},
    {"or", {0x000E, R}},
    {"sw", {0x000F, ILS}}
};

/**
 * @brief Extracts LSB 4 bits.
 */
const uint16_t LSB4_MASK = 0xFFFF;

/**
 * @brief Shifting amount to take LSB 4 bits to instruction format position.
 */
const unsigned SHIFT_OPCODE = 12, SHIFT_RS = 8, SHIFT_RT = 4, SHIFT_RD = 0;
/**
 * @brief Shifting amount to take LSB 8 bits to instruction formal position. 
 */
const unsigned SHIFT_JA = 4;

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

uint16_t convert_Rtype(string operation, string operands) {
    vector<string> args = split_str(operands, ",");
    string rd = args[0], rs = args[1], rt = args[2];

    // cout << rs << " " << rt << " " << rd << endl;
    uint16_t hexcode = 0;
    hexcode |= (instr2op[operation].first << SHIFT_OPCODE);
    hexcode |= (reg2addr[rs] << SHIFT_RS);
    hexcode |= (reg2addr[rt] << SHIFT_RT);
    hexcode |= (reg2addr[rd] << SHIFT_RD);

    return hexcode;
}

uint16_t convert_Stype(string operation, string operands) {
    vector<string> args = split_str(operands, ",");
    string rt = args[0], rs = args[1], shamt = args[2];
    uint16_t shamt_x16 = stoi(args[2]);
    
    // cout << rs << " " << rt << " " << shamt << endl;
    uint16_t hexcode = 0;
    hexcode |= (instr2op[operation].first << SHIFT_OPCODE);
    hexcode |= (reg2addr[rs] << SHIFT_RS);
    hexcode |= (reg2addr[rt] << SHIFT_RT);
    hexcode |= (shamt_x16 << SHIFT_RD);

    return hexcode;
}

uint16_t convert_Itype(string operation, string operands) {
    vector<string> args = split_str(operands, ",");
    string rt = args[0], rs = args[1], addr_imm = args[2];
    uint16_t addr_imm_x16 = stoi(args[2]);
    
    // cout << rs << " " << rt << " " << addr_imm << endl;
    uint16_t hexcode = 0;
    hexcode |= (instr2op[operation].first << SHIFT_OPCODE);
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
    uint16_t offset_x16 = stoi(offset);

    // cout << rs << " " << rt << " " << offset << endl;
    uint16_t hexcode = 0;
    hexcode |= (instr2op[operation].first << SHIFT_OPCODE);
    hexcode |= (reg2addr[rs] << SHIFT_RS);
    hexcode |= (reg2addr[rt] << SHIFT_RT);
    hexcode |= (offset_x16 << SHIFT_RD);

    return hexcode;
}

uint16_t convert_Jtype(string operation, string operands) {
    uint16_t addr_x16 = stoi(operands);
    
    // cout << operands << endl;
    uint16_t hexcode = 0;
    hexcode |= (instr2op[operation].first << SHIFT_OPCODE);
    hexcode |= (addr_x16 << SHIFT_JA);
    return hexcode;
}

uint16_t convert_mips_to_hexcode(string mips) {
    vector<string> mips_split1 = split_str(mips, " ");
    string instruction = mips_split1[0], operands = mips_split1[1];

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
            uint16_t hexcode = convert_mips_to_hexcode(mips);
            hexfile.write(reinterpret_cast<const char*>(&hexcode), sizeof(uint16_t));
            // cout << hexcode << endl; // Bytes are stored in little-endian format (LSByte first)
        } catch (int exception) {
            cerr << "Could not convert instruction" << endl;
        }    
    }

    mipsfile.close();
    hexfile.close();

    return 0;
}