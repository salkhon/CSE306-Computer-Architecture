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
 * @brief There are 7 registers. This maps register $names to their address. Extract the last 4 bits.
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
 * @brief Maps label definitions to their corresponding hex code lines, in the first pass.
 */
map<string, unsigned> mipslabel2hexline;

/**
 * @brief For debugging
 */
map<unsigned, unsigned> mipsline2hexlinestart;

vector<string> init_mipscode();
void read_all_mips(ifstream&, vector<string>&);
void first_pass_to_map_label_def_to_hexlines(vector<string>&);
string process_label(string, unsigned);
vector<string> second_pass_to_generate_hexcode(vector<string>&);
vector<string> preprocess_mips(string mips);
void dump_hexcode_to_file(vector<string>&, ofstream&);
void generate_debug_file(vector<string>&, vector<string>&);

vector<uint16_t> convert_push_pop(string, string);
uint16_t convert_mips_to_hexcode(string, string, size_t);
uint16_t convert_Rtype(string, string);
uint16_t convert_Stype(string, string);
uint16_t convert_Itype(string, string, size_t = 0);
uint16_t convert_Itype_loadstore(string, string);
uint16_t convert_Jtype(string, string);

vector<string> split_str(string, string);
void replace_substr(string&, string, string);
void trim(string&);
string convert_uint16_t_to_binstring(uint16_t);
string convert_uint16_t_to_hexstring(uint16_t);
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

    vector<string> mipscode = init_mipscode();
    read_all_mips(mipsfile, mipscode);

    first_pass_to_map_label_def_to_hexlines(mipscode);
    vector<string> hexcode = second_pass_to_generate_hexcode(mipscode);

    dump_hexcode_to_file(hexcode, hexfile);

    generate_debug_file(mipscode, hexcode);

    mipsfile.close();
    hexfile.close();

    return 0;
}

vector<string> init_mipscode() {
    vector<string> mipscode{
        "sub $zero, $zero, $zero", // first instruction skips, call this NOP
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
void read_all_mips(ifstream& mipsfile, vector<string>& mipscode) {
    string mips;
    while (getline(mipsfile, mips)) {
        mipscode.push_back(mips);
    }
}

/**
 * @brief Goes through mips lines and populates the map `mipslabel2hexline`. Hexline count is accumulated
 * by how many hex lines each mips instruction generates, and that accumulation is assigned to the defined
 * label in the order they are excountered.
 *
 * @param mipscode List of each mips code lines
 */
void first_pass_to_map_label_def_to_hexlines(vector<string>& mipscode) {
    string mips;
    for (size_t mipsline = 0, hexline = 0; mipsline < mipscode.size(); mipsline++) {
        mipsline2hexlinestart[mipsline] = hexline; // `mipscode[mipsline]` translates to hex at `hexlline`

        mips = mipscode[mipsline];
        mips = process_label(mips, hexline); // removing label from mips code

        // hardcoding which mips corresponds to how many hexline
        if (mips.starts_with("push") || mips.starts_with("pop")) {
            hexline += 2;
        } else {
            hexline++;
        }
    }
}

/**
 * @brief If provided mips contains a label definition, it maps that label definition to the provided
 * hexline. Returns the mips instruction without label, after processing it.
 *
 * @param mips Mips line
 * @param hexline Current hex code line
 * @return Mips code without label
 */
string process_label(string mips, unsigned hexline) {
    trim(mips);
    if (mips.find(':') != string::npos) {
        // label found
        vector<string> label_mips_split = split_str(mips, ":");
        mipslabel2hexline[label_mips_split[0]] = hexline; // the label definition is mapped to current hexline
        mips = label_mips_split[1]; // mips line may just contain label:, on that case [1] is ""
        trim(mips);
    }
    return mips;
}

/**
 * @brief After label definitions are mapped to their corresponding hexline, 2nd pass goes through each mips
 * line and generates corresponding hexcode in their proper instruction format. Branching references are
 * replaced with actual addresses or offset based on the `mipslabel2hexline` map.
 *
 * @param mipscode List of mips code to assemble
 * @return List of hexcode strings that were assembled from `mipscode`
 */
vector<string> second_pass_to_generate_hexcode(vector<string>& mipscode) {
    vector<string> hexcodes;
    size_t hexline = 0;
    for (string& mips : mipscode) {
        try {
            vector<string> instr_operand_split = preprocess_mips(mips);

            if (instr_operand_split.size() == 1) {
                // empty line (only label)
                // call this NOP
                instr_operand_split[0] = "sub";
                instr_operand_split.push_back("$zero,$zero,$zero");
            }

            string instruction = instr_operand_split[0], operands = instr_operand_split[1];

            if (instruction == "push" || instruction == "pop") {
                vector<uint16_t> hexcodes_x16 = convert_push_pop(instruction, operands);
                hexcodes.push_back(convert_uint16_t_to_hexstring(hexcodes_x16[0]));
                hexcodes.push_back(convert_uint16_t_to_hexstring(hexcodes_x16[1]));
                hexline += 2;
            } else {
                uint16_t hexcode = convert_mips_to_hexcode(instruction, operands, hexline);
                hexcodes.push_back(convert_uint16_t_to_hexstring(hexcode));
                hexline++;
            }
        } catch (int exception) {
            cerr << "Could not convert instruction" << endl;
        }
    }
    return hexcodes;
}

/**
 * @brief Splits the mips instruction line by instruction name, and operands.
 * Examples: add $t0, $t1, $t2 is split into {"add", "$t0,$t1,$t2"}.
 *
 * @param mips Mips to split
 * @return vector<string> List of two elements, instruction name and its operands
 */
vector<string> preprocess_mips(string mips) {
    trim(mips);
    vector<string> label_mips_split = split_str(mips, ":");
    if (mips.find(':') != string::npos) {
        vector<string> label_mips_split = split_str(mips, ":");
        mips = label_mips_split[1];
        trim(mips);
    }
    replace_substr(mips, ", ", ",");
    return split_str(mips, " ");
}

/**
 * @brief Writes `hexcodes` to `hexfile`.
 *
 * @param hexcodes List of hexcode strings
 * @param hexfile File to write hexcode into
 */
void dump_hexcode_to_file(vector<string>& hexcodes, ofstream& hexfile) {
    for (string& hexcode : hexcodes) {
        hexfile << hexcode << endl;
    }
}

void generate_debug_file(vector<string>& mipscode, vector<string>& hexcode) {
    ofstream hex_debug_file("hex_debug.txt");
    size_t hexline = 0;
    for (size_t mipsline = 0; mipsline < mipscode.size() - 1; mipsline++) {
        hex_debug_file << "[" << mipsline << "] " << mipscode[mipsline] << ":" << endl;
        while (hexline < mipsline2hexlinestart[mipsline + 1]) {
            hex_debug_file << "\t" << "[" << hexline << "] " << convert_hex_to_bin_str(hexcode[hexline]) <<
                "\t" << hexcode[hexline++] << endl;
        }
    }
    hex_debug_file << "[" << mipscode.size() - 1 << "] " << mipscode[mipscode.size() - 1] << endl;
    while (hexline < hexcode.size()) {
        hex_debug_file << "\t" << "[" << hexline << "] " << convert_hex_to_bin_str(hexcode[hexline]) <<
            "\t" << hexcode[hexline++] << endl;
    }
    hex_debug_file.close();
}

/**
 * @brief Converts provided mips to hexcode based on it's instruction type.
 *
 * @param instruction Mips instruction name
 * @param operands Mips instruction operands
 * @param hexline Current hexline being assembler
 * @return uint16_t Assembled 16 bit hexcode
 */
uint16_t convert_mips_to_hexcode(string instruction, string operands, size_t hexline) {
    if (instr2op.count(instruction) == 0) {
        throw 1;
    } else if (instr2op[instruction].second == R) {
        return convert_Rtype(instruction, operands);
    } else if (instr2op[instruction].second == S) {
        return convert_Stype(instruction, operands);
    } else if (instr2op[instruction].second == I) {
        return convert_Itype(instruction, operands, hexline);
    } else if (instr2op[instruction].second == ILS) {
        return convert_Itype_loadstore(instruction, operands);
    } else {
        return convert_Jtype(instruction, operands);
    }
}

/**
 * @brief Mips `push` and `pop` are each converted to two separate instructions.
 * Example: push $t0 -> sw $t0, 0($sp)
 *                      addi $sp, $sp, -1
 *          pop $t0 -> lw $t0, 0($sp)
 *                      addi $sp, $sp, 1
 *
 * @param operation `push` or `pop`
 * @param operands Register to push off or pop into
 * @return vector<uint16_t> List of two assembled 16 bit hexcodes
 */
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

    uint16_t hexcode = 0;
    hexcode |= instr2op[operation].first;
    hexcode |= (reg2addr[rs] << SHIFT_RS);
    hexcode |= (reg2addr[rt] << SHIFT_RT);
    hexcode |= shamt_x16;

    return hexcode;
}

uint16_t convert_Itype(string operation, string operands, size_t hexline) {
    vector<string> args = split_str(operands, ",");
    string rt = args[0], rs = args[1];

    // if mips is `beq` or `bneq`, their labels are replaced with offset to those label definitions from hexline
    // else its just a constant number.
    int16_t addr_imm_x16 =
        operation == "beq" || operation == "bneq" ?
        mipslabel2hexline[args[2]] - hexline : stoi(args[2]);

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

    uint16_t hexcode = 0;
    hexcode |= instr2op[operation].first;
    hexcode |= (reg2addr[rs] << SHIFT_RS);
    hexcode |= (reg2addr[rt] << SHIFT_RT);
    hexcode |= offset_x16;
    return hexcode;
}

uint16_t convert_Jtype(string operation, string operand) {
    uint16_t addr_x16 = mipslabel2hexline[operand]; // label is replaced with it's definition hexline

    // cout << operands << endl;
    uint16_t hexcode = 0;
    hexcode |= instr2op[operation].first;
    hexcode |= (addr_x16 << SHIFT_JA);
    return hexcode;
}

/**
 * @brief Splits `str` by the provided `delim`. This implementation may contain a trailing empty
 * string, don't rely on the .size() of the returned list for any operation.
 *
 * @param str String to split
 * @param delim Delimitter to split string by
 * @return vector<string> List of split substrings
 */
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

/**
 * @brief Converts 16 bit integers to string of 16 bits.
 *
 * @param hexnum 16 bit integer to convert
 * @return string String of bits from the provided int
 */
string convert_uint16_t_to_binstring(uint16_t hexnum) {
    stringstream sstrm;
    sstrm << bitset<16>(hexnum);
    return sstrm.str();
}

/**
 * @brief Converts 16 bit integers to a string of 4 hexadecimal digits.
 *
 * @param hexcode 16 bit integer to convert
 * @return string String of hexadecimal digits from the provided int
 */
string convert_uint16_t_to_hexstring(uint16_t hexcode) {
    stringstream sstrm;
    sstrm << setw(4) << setfill('0') << hex << hexcode;
    return sstrm.str();
}

/**
 * @brief Converts string representation of hexadecimal number to it's corresponding
 * binary string representation.
 *
 * @param hex_str String of hexadecimals digits
 * @return string String of bits
 */
string convert_hex_to_bin_str(string hex_str) {
    uint16_t hex = stoi(hex_str, nullptr, 16);
    return convert_uint16_t_to_binstring(hex);
}
