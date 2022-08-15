#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <iomanip>
#include <bitset>
#include <sstream>

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
 * @brief Assign STACK_SEGMENT to $sp as the first instruction assembled, by default. Registers are 4 bits, so
 * stack segment has to be within (0, 15)
 */
const uint8_t STACK_SEGMENT = 0x00;

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
const unsigned SHIFT_RS = 8, SHIFT_RT = 4;
/**
 * @brief Shifting amount to take LSB 8 bits to instruction formal position.
 */
const unsigned SHIFT_JA = 4;

/**
 * @brief Labels can be referred before they are defined sequentially. So label to line mappings
 * need to be done in two passes. First pass assembles mips to instruction codes without setting
 * the offset or address on BEQ, BNEQ, J instructions, those fields remain blank. It populates the
 * label2line map with with label to corresponding definition line mapping in the instruction code.
 *
 * On the second pass, it goes through the BEQ, BNEQ, J instructions and fill in their blank
 * offset and address fields with the mapped lines correspoinding to their labels.
 */

 /**
  * @brief Maps labels to their corresponding hex code lines.
  */
map<string, unsigned> label2line;
/**
 * @brief Labels are added to the queue in the order they are encountered in beq, bneq and j instructions.
 * They are filled on the 2nd pass, in that order.
 */
vector<string> labels_to_fill;

/**
 * @brief For debugging
 */
map<unsigned, unsigned> mipsline2hexline;

vector<string> init();
vector<string> read_all_mips(ifstream&);
vector<string> preprocess_mips(string, unsigned, unsigned);
vector<string> generate_binary_instr_without_labels(vector<string>&);
void generate_hex_instr_with_labels(vector<string>&);
void dump_hexcode_to_file(vector<string>&, ofstream&);
void generate_debug_file(vector<string>&, vector<string>&);

vector<uint16_t> convert_push_pop(string, string);
uint16_t convert_mips_to_hexcode(string, string);
uint16_t convert_Rtype(string, string);
uint16_t convert_Stype(string, string);
uint16_t convert_Itype(string, string);
uint16_t convert_Itype_loadstore(string, string);
uint16_t convert_Jtype(string, string);

// utils
vector<string> split_str(string, string);
void replace_substr(string&, string, string);
void trim(string&);
string convert_uint16_to_binstring(uint16_t);
string convert_uint16_to_hexstring(uint16_t);
string convert_hex_to_bin_str(string);

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

    ofstream hexfile("hex.txt");
    if (!hexfile.is_open()) {
        cerr << "Error: Could not open write file: hex.txt" << endl;
        return 1;
    }

    vector<string> mipscode = init();
    vector<string> read_mips = read_all_mips(mipsfile);
    mipscode.insert(mipscode.end(), read_mips.begin(), read_mips.end());

    vector<string> hexcode = generate_binary_instr_without_labels(mipscode);
    generate_hex_instr_with_labels(hexcode);
    dump_hexcode_to_file(hexcode, hexfile);

    generate_debug_file(mipscode, hexcode);

    mipsfile.close();
    hexfile.close();

    return 0;
}

vector<string> init() {
    vector<string> mipscode{
        "sub $zero, $zero, $zero",
        "sub $sp, $sp, $sp",
        "addi $sp, $sp," + to_string(STACK_SEGMENT & 0x0F)
    };
    return mipscode;
}

/**
 * @brief Reads all mips code from provided file.
 *
 * @param mipsfile File to read mips code from
 * @return vector<string> List of strings corresponding to mips code lines
 */
vector<string> read_all_mips(ifstream& mipsfile) {
    vector<string> mips_code;
    string mips;
    while (getline(mipsfile, mips)) {
        mips_code.push_back(mips);
    }
    return mips_code;
}

/**
 * @brief Splits instruction in label and instruction. If instruction has label, label2line map is populated.
 * Then splits instruction by instruction name and operands.
 *
 * @param mips Mips line
 * @param hex_line Current hex code line
 * @return vector<string> List of 2 elements, instruction name and its arguments in string
 */
vector<string> preprocess_mips(string mips, unsigned mipsline, unsigned hexline) {
    mipsline2hexline[mipsline] = hexline;
    trim(mips);
    vector<string> label_mips_split = split_str(mips, ":");
    if (label_mips_split.size() > 1) {
        // there's a label
        label2line[label_mips_split[0]] = hexline;
        mips = label_mips_split[1];
        trim(mips);
    }

    replace_substr(mips, ", ", ",");

    vector<string> instr_arg_split = split_str(mips, " ");
    return instr_arg_split;
}

/**
 * @brief Generates hexcode from list of mips lines. Also populates the global label2line map,
 * which maps which hex line each label corresponds to. This mapping cannot be done dynamically
 * without generating hex.
 *
 * @param mipscode List of mips code lines
 * @return vector<string> List of hex code lines, without beq, bneq instruction offset and j instruction
 * address.
 */
vector<string> generate_binary_instr_without_labels(vector<string>& mipscodes) {
    vector<string> hexcodes;

    for (unsigned mipsline = 0, hexline = 0; mipsline < mipscodes.size(); mipsline++) {
        try {
            vector<string> instr_operand_split = preprocess_mips(mipscodes[mipsline], mipsline, hexline);
            string instruction = instr_operand_split[0], operands = instr_operand_split[1];

            if (instruction == "push" || instruction == "pop") {
                vector<uint16_t> hexcodes_x16 = convert_push_pop(instruction, operands);
                hexcodes.push_back(convert_uint16_to_binstring(hexcodes_x16[0]));
                hexcodes.push_back(convert_uint16_to_binstring(hexcodes_x16[1]));
                hexline += 2;
            } else {
                uint16_t hexcode = convert_mips_to_hexcode(instruction, operands);
                hexcodes.push_back(convert_uint16_to_binstring(hexcode));
                hexline++;
            }
        } catch (int exception) {
            cerr << "Could not convert instruction" << endl;
        }
    }
    return hexcodes;
}

/**
 * @brief Populates the immediate field of beq, bneq instructions, and the address field of j instructions
 * from the label2line map generated by the function `generate_binary_instr_without_labels`.
 * So `generate_binary_instr_without_labels` has to be executed before this function can execute.
 *
 * @param hex_wo_label
 * @return vector<string>
 */
void generate_hex_instr_with_labels(vector<string>& instr_without_label) {
    stringstream sstrm;
    uint16_t hex_x16;
    int16_t offset_or_addr;
    for (int hexline = 0; hexline < instr_without_label.size(); hexline++) {
        hex_x16 = stoi(instr_without_label[hexline], nullptr, 2);

        if (instr_without_label[hexline].find("0100") == 0) {
            // jump instruction
            offset_or_addr = label2line[labels_to_fill[0]];
            labels_to_fill.erase(labels_to_fill.begin());

            hex_x16 |= offset_or_addr << SHIFT_JA;
        } else if (instr_without_label[hexline].find("0110") == 0 ||
            instr_without_label[hexline].find("1010") == 0) {
            // bneq or beq instruction
            offset_or_addr = label2line[labels_to_fill[0]] - hexline;
            if (offset_or_addr > 7 || offset_or_addr < -8) {
                cerr << "WARNING: offset overflow detected at hex line " << hexline << endl;
            }
            if (offset_or_addr < 0) {
                offset_or_addr &= 0x000F; // zero-ing every other bits than LSB 4 bits. offset range (-8, 7)
            }
            labels_to_fill.erase(labels_to_fill.begin());

            hex_x16 |= offset_or_addr;
        }

        instr_without_label[hexline] = convert_uint16_to_hexstring(hex_x16);
        sstrm.clear();
    }
}

void dump_hexcode_to_file(vector<string>& hexcodes, ofstream& hexfile) {
    for (string& hexcode : hexcodes) {
        hexfile << hexcode << endl;
    }
}

void generate_debug_file(vector<string>& mipscode, vector<string>& hexcode) {
    ofstream hex_debug_file("hex_debug.txt");
    for (unsigned mipsline = 1, hexline = 0; mipsline < mipscode.size(); mipsline++) {
        hex_debug_file << "[" << mipsline - 1 << "] " << mipscode[mipsline - 1] << ":" << endl;
        while (hexline != mipsline2hexline[mipsline]) {
            hex_debug_file << "\t" << "[" << hexline << "] " << convert_hex_to_bin_str(hexcode[hexline++]) << endl;
        }
    }
    hex_debug_file << "[" << mipscode.size() - 1 << "] " << mipscode[mipscode.size() - 1] << endl;
    for (unsigned hexline = mipsline2hexline[mipscode.size() - 1]; hexline < hexcode.size(); hexline++) {
        hex_debug_file << "\t" << "[" << hexline << "] " << convert_hex_to_bin_str(hexcode[hexline]) << endl;
    }
    hex_debug_file.close();
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
    hexcode |= reg2addr[rd];

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
    hexcode |= shamt_x16;

    return hexcode;
}

uint16_t convert_Itype(string operation, string operands) {
    vector<string> args = split_str(operands, ",");
    string rt = args[0], rs = args[1];

    int16_t addr_imm_x16;
    if (operation == "beq" || operation == "bneq") {
        // if beq or bne, label will be filled in a later pass (bit0 to bit3)
        addr_imm_x16 = 0;
        labels_to_fill.push_back(args[2]);
    } else {
        addr_imm_x16 = stoi(args[2]);
    }

    // cout << rs << " " << rt << " " << addr_imm_x16 << endl;
    uint16_t hexcode = 0;
    hexcode |= instr2op[operation].first;
    hexcode |= (reg2addr[rs] << SHIFT_RS);
    hexcode |= (reg2addr[rt] << SHIFT_RT);
    hexcode |= addr_imm_x16;
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
    hexcode |= offset_x16;
    return hexcode;
}

uint16_t convert_Jtype(string operation, string operands) {
    uint16_t addr_x16 = 0; // j addresses will be filled in a later pass (bit4 to bit 11)
    labels_to_fill.push_back(operands);

    // cout << operands << endl;
    uint16_t hexcode = 0;
    hexcode |= instr2op[operation].first;
    hexcode |= (addr_x16 << SHIFT_JA);
    return hexcode;
}

// utils
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

/**
 * @brief Replaces subject string containing target substring, with the replacement string.
 *
 * @param subject String to replace in
 * @param target Substring to replace
 * @param replacement String to replace target substring with
 */
void replace_substr(string& subject, string target, string replacement) {
    size_t pos = 0;
    while ((pos = subject.find(target, pos)) != string::npos) {
        subject.replace(pos, target.length(), replacement);
        pos += replacement.length();
    }
}

/**
 * @brief Trims whitespace from string.
 *
 * @param str String to trim
 */
void trim(string& str) {
    string ws = " \t";
    str.erase(0, str.find_first_not_of(ws));
    str.erase(str.find_last_not_of(ws) + 1);
}

string convert_uint16_to_binstring(uint16_t hexnum) {
    stringstream sstrm;
    sstrm << bitset<16>(hexnum);
    return sstrm.str();
}

string convert_uint16_to_hexstring(uint16_t hexcode) {
    stringstream sstrm;
    sstrm << setw(4) << setfill('0') << hex << hexcode;
    return sstrm.str();
}

string convert_hex_to_bin_str(string hex_str) {
    uint16_t hex = stoi(hex_str, nullptr, 16);
    return convert_uint16_to_binstring(hex);
}
