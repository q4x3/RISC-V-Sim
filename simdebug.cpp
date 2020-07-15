#include <iostream>
#include <cstdio>
#include <cstring>
#define b_31_11 0xFFFFF800U
#define b_31_12 0xFFFFF000U
#define b_31_20 0xFFF00000U
#define b_31_25 0xFE000000U
#define b_11_8  0x00000F00U
#define b_24_21 0x01E00000U
#define b_11_7  0x00000F80U
#define b_24_20 0x01F00000U
#define b_30_25 0x7E000000U
#define b_19_12 0x000FF000U
#define b_30_21 0x7FE00000U

using namespace std;

namespace SIM {
    enum INST {
    LUI, AUIPC,                                             // U-type
    JALR,                                                   // I-type
    JAL,                                                    // J-type
    BEQ, BNE, BLT, BGE, BLTU, BGEU,                         // B-type
    LB, LH, LW, LBU, LHU,                                   // I-type
    SB, SH, SW,                                             // S-type
    ADDI, SLTI, SLTIU, XORI, ORI, ANDI, SLLI, SRLI, SRAI,   // I-type
    ADD, SUB, SLL, SLT, SLTU, XOR, SRL, SRA, OR, AND        // R-type
    };
    const char INST_string[40][15] = {
    "LUI  ",  "AUIPC", "JALR ",  "JAL  ", "BEQ  ",   "BNE  ",   "BLT  ", "BGE  ",
    "BLTU ", "BGEU ",  "LB   ",   "LH   ",   "LW   ",    "LBU  ",   "LHU  ", "SB   ",
    "SH   ",   "SW   ",    "ADDI ", "SLTI ", "SLTIU", "XORI ",  "ORI  ", "ANDI ",
    "SLLI ", "SRLI ",  "SRAI ", "ADD  ",  "SUB  ",   "SLL  ",   "SLT  ", "SLTU ",
    "XOR  ",  "SRL  ",   "SRA  ",  "OR   ",   "AND  ",   "NOOP "};
    class simulator {
        public:
        uint8_t mem[0xFFFFF];
        uint32_t code;
        int reg[32], pc, ext_imm, rs1, rs2, rd, exe_tmp, pc_tmp, mem_tmp, ALU;
        bool pc_jump;
        char buffer[10];
        INST opt;
        // CONSTRUCT
        simulator() {
            code = 0;
            pc = 0, ext_imm = 0, rs1 = 0, rs2 = 0, rd = 0, exe_tmp = 0, pc_jump = 0, pc_tmp = 0, mem_tmp = 0, ALU = 0;
            for(int i = 0;i < 32;++ i) {
                reg[i] = 0;
            }
        }
        // READ
        void read() {
            int addr = 0;
            while(scanf("%s", buffer) != EOF) {
                if(buffer[0] == '@') {
                    sscanf(buffer + 1, "%x", &addr);
                } else {
                    // 从字符串中当作十六进制读入，存储为unsigned char
                    sscanf(buffer, "%x", &mem[addr ++]);
                    scanf("%x", &mem[addr ++]);
                    scanf("%x", &mem[addr ++]);
                    scanf("%x", &mem[addr ++]);
                }
            }
        }
        // IMMEDIATE EXTENT
        int ext_I(const uint32_t &code) {
            return (code >> 31) ? ((code >> 20) | b_31_11) : (code >> 20);
        }
        int ext_S(const uint32_t &code) {
            int tmp = (code >> 7) & 1U;
            tmp |= (((code & b_11_8) >> 8) << 1);
            tmp |= (((code & b_30_25) >> 25) << 5);
            return (code >> 31) ? (tmp | b_31_11) : tmp;
        }
        int ext_B(const uint32_t &code) {
            int tmp = (((code & b_11_8) >> 8) << 1);
            tmp |= (((code & b_30_25) >> 25) << 5);
            tmp |= (((code >> 7) & 1U) << 11);
            return (code >> 31) ? (tmp | b_31_12) : tmp;
        }
        int ext_U(const uint32_t &code) {
            return code & b_31_12;
        }
        int ext_J(const uint32_t &code) {
            int tmp = (((code & b_24_21) >> 21) << 1);
            tmp |= (((code & b_30_25) >> 25) << 5);
            tmp |= (((code >> 20) & 1U) << 11);
            tmp |= (code & b_19_12);
            return (code >> 31) ? (tmp | b_31_20) : tmp;
        }
        // INSTRUCTION FETCH
        void fetch() {
            memcpy(&code, mem + pc, 4);
        }
        // INSTRUCTION DECODE
        void decode() {
            uint32_t opcode = code & 0x7f,
                     funct3 = (code >> 12) & 0x7, funct7 = code >> 25;
            rs1 = 0, rs2 = 0, rd = 0, exe_tmp = 0, pc_jump = 0, pc_tmp = 0, mem_tmp = 0, ALU = 0;
            switch(opcode) {
            case 0x37:
                opt = LUI;
                rd = (code >> 7) & 0x1F;
                ext_imm = ext_U(code);
                break;
            case 0x17:
                opt = AUIPC;
                rd = (code >> 7) & 0x1F;
                ext_imm = ext_U(code);
                break;
            case 0x6F:
                opt = JAL;
                rd = (code >> 7) & 0x1F;
                ext_imm = ext_J(code);
                break;
            case 0x67:
                opt = JALR;
                rd = (code >> 7) & 0x1F;
                rs1 = (code >> 15) & 0x1F;
                ext_imm = ext_I(code);
                break;
            case 0x63:
                switch(funct3) {
                case 0x0:
                    opt = BEQ;
                    break;
                case 0x1:
                    opt = BNE;
                    break;
                case 0x4:
                    opt = BLT;
                    break;
                case 0x5:
                    opt = BGE;
                    break;
                case 0x6:
                    opt = BLTU;
                    break;
                case 0x7:
                    opt = BGEU;
                    break;
                }
                rs1 = (code >> 15) & 0x1F;
                rs2 = (code >> 20) & 0x1F;
                ext_imm = ext_B(code);
                break;
            case 0x3:
                switch(funct3) {
                case 0x0:
                    opt = LB;
                    break;
                case 0x1:
                    opt = LH;
                    break;
                case 0x2:
                    opt = LW;
                    break;
                case 0x4:
                    opt = LBU;
                    break;
                case 0x5:
                    opt = LHU;
                    break;
                }
                rd = (code >> 7) & 0x1F;
                rs1 = (code >> 15) & 0x1F;
                ext_imm = ext_I(code);
                break;
            case 0x23:
                switch(funct3) {
                case 0x0:
                    opt = SB;
                    break;
                case 0x1:
                    opt = SH;
                    break;
                case 0x2:
                    opt = SW;
                    break;
                }
                rs1 = (code >> 15) & 0x1F;
                rs2 = (code >> 20) & 0x1F;
                ext_imm = ext_S(code);
                break;
            case 0x13:
                switch(funct3) {
                case 0x0:
                    opt = ADDI;
                    break;
                case 0x2:
                    opt = SLTI;
                    break;
                case 0x3:
                    opt = SLTIU;
                    break;
                case 0x4:
                    opt = XORI;
                    break;
                case 0x6:
                    opt = ORI;
                    break;
                case 0x7:
                    opt = ANDI;
                    break;
                case 0x1:
                    opt = SLLI;
                    break;
                case 0x5:
                    switch(funct7) {
                    case 0x0:
                        opt = SRLI;
                        // to do
                        break;
                    case 0x20:
                        opt = SRAI;
                        // to do
                        break;
                    }
                    break;
                }
                rd = (code >> 7) & 0x1F;
                rs1 = (code >> 15) & 0x1F;
                ext_imm = ext_I(code);
                break;
            case 0x33:
                switch(funct3) {
                case 0x0:
                    switch(funct7) {
                    case 0x0:
                        opt = ADD;
                        break;
                    case 0x20:
                        opt = SUB;
                        break;
                    }
                    break;
                case 0x1:
                    opt = SLL;
                    break;
                case 0x2:
                    opt = SLT;
                    break;
                case 0x3:
                    opt = SLTU;
                    break;
                case 0x4:
                    opt = XOR;
                    break;
                case 0x5:
                    switch(funct7) {
                    case 0x0:
                        opt = SRL;
                        break;
                    case 0x20:
                        opt = SRA;
                        break;
                    }
                    break;
                case 0x6:
                    opt = OR;
                    break;
                case 0x7:
                    opt = AND;
                    break;
                }
                rd = (code >> 7) & 0x1F;
                rs1 = (code >> 15) & 0x1F;
                rs2 = (code >> 20) & 0x1F;
                break;
            }
        }
        // EXECUTE
        void execute() {
            switch(opt) {
            case LUI:
                ALU = ext_imm;
                pc_jump = 0;
                break;
            case AUIPC:
                ALU = pc + ext_imm;
                pc_jump = 0;
                break;
            case JAL:
                ALU = pc + 4;
                pc_jump = 1;
                pc_tmp = pc + ext_imm;
                break;
            case JALR:
                ALU = pc + 4;
                pc_jump = 1;
                pc_tmp = reg[rs1] + ext_imm;
                break;
            case BEQ:
                if(reg[rs1] == reg[rs2]) {
                    pc_jump = 1;
                    pc_tmp = pc + ext_imm;
                } else {
                    pc_jump = 0;
                }
                break;
            case BNE:
                if(reg[rs1] != reg[rs2]) {
                    pc_jump = 1;
                    pc_tmp = pc + ext_imm;
                } else {
                    pc_jump = 0;
                }
                break;
            case BLT:
                if(reg[rs1] < reg[rs2]) {
                    pc_jump = 1;
                    pc_tmp = pc + ext_imm;
                } else {
                    pc_jump = 0;
                }
                break;
            case BGE:
                if(reg[rs1] >= reg[rs2]) {
                    pc_jump = 1;
                    pc_tmp = pc + ext_imm;
                } else {
                    pc_jump = 0;
                }
                break;
            case BLTU:
                if(uint32_t(reg[rs1]) < uint32_t(reg[rs2])) {
                    pc_jump = 1;
                    pc_tmp = pc + ext_imm;
                } else {
                    pc_jump = 0;
                }
                break;
            case BGEU:
                if(uint32_t(reg[rs1]) >= uint32_t(reg[rs2])) {
                    pc_jump = 1;
                    pc_tmp = pc + ext_imm;
                } else {
                    pc_jump = 0;
                }
                break;
            case LB:
                exe_tmp = reg[rs1] + ext_imm;
                break;
            case LBU:
                exe_tmp = reg[rs1] + ext_imm;
                break;
            case LH:
                exe_tmp = reg[rs1] + ext_imm;
                break;
            case LHU:
                exe_tmp = reg[rs1] + ext_imm;
                break;
            case LW:
                exe_tmp = reg[rs1] + ext_imm;
                break;
            case SB:
                exe_tmp = reg[rs1] + ext_imm;
                break;
            case SH:
                exe_tmp = reg[rs1] + ext_imm;
                break;
            case SW:
                exe_tmp = reg[rs1] + ext_imm;
                break;
            case ADDI:
                ALU = reg[rs1] + ext_imm;
                break;
            case SLTI:
                ALU = reg[rs1] < ext_imm;
                break;
            case SLTIU:
                ALU = uint32_t(reg[rs1]) < uint32_t(ext_imm);
                break;
            case XORI:
                ALU = reg[rs1] ^ ext_imm;
                break;
            case ORI:
                ALU = reg[rs1] | ext_imm;
                break;
            case ANDI:
                ALU = reg[rs1] & ext_imm;
                break;
            case SLLI:
                ALU = reg[rs1] << ext_imm;
                break;
            case SRLI:
                ALU = uint32_t(reg[rs1]) >> uint32_t(ext_imm);
                break;
            case SRAI:
                ALU = reg[rs1] >> ext_imm;
                break;
            case ADD:
                ALU = reg[rs1] + reg[rs2];
                break;
            case SUB:
                ALU = reg[rs1] - reg[rs2];
                break;
            case SLL:
                ALU = reg[rs1] << (reg[rs2] & 0x1F);
                break;
            case SLT:
                ALU = reg[rs1] < reg[rs2];
                break;
            case SLTU:
                ALU = uint32_t(reg[rs1]) < uint32_t(reg[rs2]);
                break;
            case XOR:
                ALU = reg[rs1] ^ reg[rs2];
                break;
            case SRL:
                ALU = uint32_t(reg[rs1]) >> uint32_t(reg[rs2] & 0x1F);
                break;
            case SRA:
                ALU = reg[rs1] >> (reg[rs2] & 0x1F);
                break;
            case OR:
                ALU = reg[rs1] | reg[rs2];
                break;
            case AND:
                ALU = reg[rs1] | reg[rs2];
                break;
            }
        }
        // MEMORY ACCESS
        void memacc() {
            switch(opt) {
            case LB: {
                int8_t tmp;
                memcpy(&tmp, mem + exe_tmp, 1);
                ALU = tmp;
                break;
            }
            case LBU: {
                uint8_t tmp;
                memcpy(&tmp, mem + exe_tmp, 1);
                ALU = tmp;
                break;
            }
            case LH: {
                int16_t tmp;
                memcpy(&tmp, mem + exe_tmp, 2);
                ALU = tmp;
                break;
            }
            case LHU: {
                uint16_t tmp;
                memcpy(&tmp, mem + exe_tmp, 2);
                ALU = tmp;
                break;
            }
            case LW: {
                int32_t tmp;
                memcpy(&tmp, mem + exe_tmp, 4);
                ALU = tmp;
                break;
            }
            case SB: {
                memcpy(mem + exe_tmp, &reg[rs2], 1);
                break;
            }
            case SH: {
                memcpy(mem + exe_tmp, &reg[rs2], 2);
                break;
            }
            case SW: {
                memcpy(mem + exe_tmp, &reg[rs2], 4);
                break;
            }
            }
        }
        // WRITE BACK
        void wrback() {
            if(rd != 0) {
                reg[rd] = ALU;
            }
        }
        void solve() {
            read();
            while(1) {
                fetch();
                if(code == 0x0FF00513U) {
                    printf("\nans\t%d\n", ((unsigned int)reg[10]) & 255u);
                    break;
                }
                decode();
                execute();
                memacc();
                wrback();
                std::cout << INST_string[opt] << '\t' << pc << '\t';
                for(int i = 1;i < 32;++ i)
                    std::cout << reg[i] << '\t';
                std::cout << std::endl;
                // printf("\t%d\t%d\t%d\t%d\t%d\t%d\n", pc, reg[1], reg[2], reg[10], reg[14], reg[15]);
                if(pc_jump == 0) pc += 4;
                else pc = pc_tmp;
            }
        }
    };
};

int main() {
    SIM::simulator sim;
    freopen("a.in", "r", stdin);
    freopen("b.out", "w", stdout);
    // printf("pc\topt\tx1\tx2\tx10\tx14\tx15\n");
    sim.solve();
    // printf("%d\n", sim.counter);
    return 0;
}