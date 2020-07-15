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
    ADD, SUB, SLL, SLT, SLTU, XOR, SRL, SRA, OR, AND,       // R-type
    NOOP
    };
    const char INST_string[40][15] = {
    "LUI  ",  "AUIPC", "JALR ",  "JAL  ", "BEQ  ",   "BNE  ",   "BLT  ", "BGE  ",
    "BLTU ", "BGEU ",  "LB   ",   "LH   ",   "LW   ",    "LBU  ",   "LHU  ", "SB   ",
    "SH   ",   "SW   ",    "ADDI ", "SLTI ", "SLTIU", "XORI ",  "ORI  ", "ANDI ",
    "SLLI ", "SRLI ",  "SRAI ", "ADD  ",  "SUB  ",   "SLL  ",   "SLT  ", "SLTU ",
    "XOR  ",  "SRL  ",   "SRA  ",  "OR   ",   "AND  ",   "NOOP "};
    class simulator {
        public:
        uint8_t mem[0xFFFFF], bicounter[1 << 8];
        uint32_t code;
        int reg[32], pc, endcount, tot_pre, correct_pre;
        char buffer[10];
        INST opt;
        bool endflag, jump;
        struct seg {
            uint32_t code;
            int curpc, ext_imm, rs1, rs2, rd, pc_tmp, ALU, regrs1, regrs2;
            bool pc_jump, jump, bubble;
            INST opt;
            seg() {
                code = 0;
                curpc = 0, ext_imm = 0, rs1 = 0, rs2 = 0, rd = 0, pc_tmp = 0, ALU = 0, regrs1 = 0, regrs2 = 0, pc_jump = 0, jump = 0;
                opt = NOOP;
            }
        } IF, ID, EX, MEM, WB;
        // CONSTRUCT
        simulator() {
            code = 0;
            pc = 0;
            endflag = 0;
            endcount = 0;
            tot_pre = 0;
            correct_pre = 0;
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
        // GET PREDICT
        void predict() {
            IF.jump = bicounter[(IF.curpc >> 2) & 0xff] & 2U;
            if(IF.jump) {
                pc += ext_B(IF.code);
            } else {
                pc += 4;
            }
        }
        // INSTRUCTION FETCH
        void fetch() {
            IF.jump = 0;
            if(IF.code == 0x0FF00513U && EX.bubble != 1) {
                endflag = 1;
                return;
            }
            memcpy(&(IF.code), mem + pc, 4);
            IF.curpc = pc;
            if((IF.code & 0x7fU) == 0x63U) {
                predict();
            } else {
                pc += 4;
            }
        }
        // INSTRUCTION DECODE
        void decode() {
            INST lastOpt = ID.opt;
            uint32_t opcode = IF.code & 0x7f,
                     funct3 = (IF.code >> 12) & 0x7, funct7 = IF.code >> 25;
            ID.opt = NOOP, ID.rd = 0, ID.rs1 = 0, ID.rs2 = 0, ID.ext_imm = 0, ID.regrs1 = 0, ID.regrs2 = 0;
            ID.curpc = IF.curpc;
            ID.code = IF.code;
            ID.jump = IF.jump;
            if(ID.code == 0x0FF00513U) {
                return;
            }
            switch(opcode) {
            case 0x37:
                ID.opt = LUI;
                ID.rd = (IF.code >> 7) & 0x1F;
                ID.ext_imm = ext_U(IF.code);
                break;
            case 0x17:
                ID.opt = AUIPC;
                ID.rd = (IF.code >> 7) & 0x1F;
                ID.ext_imm = ext_U(IF.code);
                break;
            case 0x6F:
                ID.opt = JAL;
                ID.rd = (IF.code >> 7) & 0x1F;
                ID.ext_imm = ext_J(IF.code);
                pc = ID.curpc + ID.ext_imm;
                break;
            case 0x67:
                ID.opt = JALR;
                ID.rd = (IF.code >> 7) & 0x1F;
                ID.rs1 = (IF.code >> 15) & 0x1F;
                ID.regrs1 = reg[ID.rs1];
                ID.ext_imm = ext_I(IF.code);
                // 上两条指令检验
                if(ID.rs1 == MEM.rd && ID.rs1 != 0 && MEM.opt != NOOP) {
                    ID.regrs1 = MEM.ALU;
                }
                // 上一条指令检验
                if(ID.rs1 == EX.rd && ID.rs1 != 0 && lastOpt != NOOP) {
                    if(EX.opt == LB || EX.opt == LH || EX.opt == LW || EX.opt == LBU || EX.opt == LHU) {
                        wrback();
                        memacc();
                        // 插入空指令
                        EX.opt = NOOP;
                        EX.rd = 0;
                        EX.rs1 = 0;
                        EX.rs2 = 0;
                        EX.pc_jump = 0;
                        EX.curpc = 0;
                        ID.regrs1 = MEM.ALU;
                    } else {
                        ID.regrs1 = EX.ALU;
                    }
                }
                pc = ID.regrs1 + ID.ext_imm;
                break;
            case 0x63:
                switch(funct3) {
                case 0x0:
                    ID.opt = BEQ;
                    break;
                case 0x1:
                    ID.opt = BNE;
                    break;
                case 0x4:
                    ID.opt = BLT;
                    break;
                case 0x5:
                    ID.opt = BGE;
                    break;
                case 0x6:
                    ID.opt = BLTU;
                    break;
                case 0x7:
                    ID.opt = BGEU;
                    break;
                }
                ID.rs1 = (IF.code >> 15) & 0x1F;
                ID.rs2 = (IF.code >> 20) & 0x1F;
                ID.ext_imm = ext_B(IF.code);
                ID.regrs1 = reg[ID.rs1];
                ID.regrs2 = reg[ID.rs2];
                // 上两条指令检验
                if(ID.rs1 == MEM.rd && ID.rs1 != 0 && MEM.opt != NOOP) {
                    ID.regrs1 = MEM.ALU;
                }
                // 上两条指令检验
                if(ID.rs2 == MEM.rd && ID.rs2 != 0 && MEM.opt != NOOP) {
                    ID.regrs2 = MEM.ALU;
                }
                // 上一条指令检验
                if(ID.rs1 == EX.rd && ID.rs1 != 0 && lastOpt != NOOP) {
                    if(EX.opt == LB || EX.opt == LH || EX.opt == LW || EX.opt == LBU || EX.opt == LHU) {
                        wrback();
                        memacc();
                        // 插入空指令
                        EX.opt = NOOP;
                        EX.rd = 0;
                        EX.rs1 = 0;
                        EX.rs2 = 0;
                        EX.pc_jump = 0;
                        EX.curpc = 0;
                        ID.regrs1 = MEM.ALU;
                    } else {
                        ID.regrs1 = EX.ALU;
                    }
                }
                // 上一条指令检验
                if(ID.rs2 == EX.rd && ID.rs2 != 0 && lastOpt != NOOP) {
                    if(EX.opt == LB || EX.opt == LH || EX.opt == LW || EX.opt == LBU || EX.opt == LHU) {
                        wrback();
                        memacc();
                        // 插入空指令
                        EX.opt = NOOP;
                        EX.rd = 0;
                        EX.rs1 = 0;
                        EX.rs2 = 0;
                        EX.pc_jump = 0;
                        EX.curpc = 0;
                        ID.regrs2 = MEM.ALU;
                    } else {
                        ID.regrs2 = EX.ALU;
                    }
                }
                break;
            case 0x3:
                switch(funct3) {
                case 0x0:
                    ID.opt = LB;
                    break;
                case 0x1:
                    ID.opt = LH;
                    break;
                case 0x2:
                    ID.opt = LW;
                    break;
                case 0x4:
                    ID.opt = LBU;
                    break;
                case 0x5:
                    ID.opt = LHU;
                    break;
                }
                ID.rd = (IF.code >> 7) & 0x1F;
                ID.rs1 = (IF.code >> 15) & 0x1F;
                ID.ext_imm = ext_I(IF.code);
                ID.regrs1 = reg[ID.rs1];
                // 上两条指令检验
                if(ID.rs1 == MEM.rd && ID.rs1 != 0 && MEM.opt != NOOP) {
                    ID.regrs1 = MEM.ALU;
                }
                // 上一条指令检验
                if(ID.rs1 == EX.rd && ID.rs1 != 0 && lastOpt != NOOP) {
                    if(EX.opt == LB || EX.opt == LH || EX.opt == LW || EX.opt == LBU || EX.opt == LHU) {
                        wrback();
                        memacc();
                        // 插入空指令
                        EX.opt = NOOP;
                        EX.rd = 0;
                        EX.rs1 = 0;
                        EX.rs2 = 0;
                        EX.pc_jump = 0;
                        EX.curpc = 0;
                        ID.regrs1 = MEM.ALU;
                    } else {
                        ID.regrs1 = EX.ALU;
                    }
                }
                break;
            case 0x23:
                switch(funct3) {
                case 0x0:
                    ID.opt = SB;
                    break;
                case 0x1:
                    ID.opt = SH;
                    break;
                case 0x2:
                    ID.opt = SW;
                    break;
                }
                ID.rs1 = (IF.code >> 15) & 0x1F;
                ID.rs2 = (IF.code >> 20) & 0x1F;
                ID.ext_imm = ext_S(IF.code);
                ID.regrs1 = reg[ID.rs1];
                ID.regrs2 = reg[ID.rs2];
                // 上两条指令检验
                if(ID.rs1 == MEM.rd && ID.rs1 != 0 && MEM.opt != NOOP) {
                    ID.regrs1 = MEM.ALU;
                }
                // 上两条指令检验
                if(ID.rs2 == MEM.rd && ID.rs2 != 0 && MEM.opt != NOOP) {
                    ID.regrs2 = MEM.ALU;
                }
                // 上一条指令检验
                if(ID.rs1 == EX.rd && ID.rs1 != 0 && lastOpt != NOOP) {
                    if(EX.opt == LB || EX.opt == LH || EX.opt == LW || EX.opt == LBU || EX.opt == LHU) {
                        wrback();
                        memacc();
                        // 插入空指令
                        EX.opt = NOOP;
                        EX.rd = 0;
                        EX.rs1 = 0;
                        EX.rs2 = 0;
                        EX.pc_jump = 0;
                        EX.curpc = 0;
                        ID.regrs1 = MEM.ALU;
                    } else {
                        ID.regrs1 = EX.ALU;
                    }
                }
                // 上一条指令检验
                if(ID.rs2 == EX.rd && ID.rs2 != 0 && lastOpt != NOOP) {
                    if(EX.opt == LB || EX.opt == LH || EX.opt == LW || EX.opt == LBU || EX.opt == LHU) {
                        wrback();
                        memacc();
                        // 插入空指令
                        EX.opt = NOOP;
                        EX.rd = 0;
                        EX.rs1 = 0;
                        EX.rs2 = 0;
                        EX.pc_jump = 0;
                        EX.curpc = 0;
                        ID.regrs2 = MEM.ALU;
                    } else {
                        ID.regrs2 = EX.ALU;
                    }
                }
                break;
            case 0x13:
                switch(funct3) {
                case 0x0:
                    ID.opt = ADDI;
                    break;
                case 0x2:
                    ID.opt = SLTI;
                    break;
                case 0x3:
                    ID.opt = SLTIU;
                    break;
                case 0x4:
                    ID.opt = XORI;
                    break;
                case 0x6:
                    ID.opt = ORI;
                    break;
                case 0x7:
                    ID.opt = ANDI;
                    break;
                case 0x1:
                    ID.opt = SLLI;
                    break;
                case 0x5:
                    switch(funct7) {
                    case 0x0:
                        ID.opt = SRLI;
                        break;
                    case 0x20:
                        ID.opt = SRAI;
                        break;
                    }
                    break;
                }
                ID.rd = (IF.code >> 7) & 0x1F;
                ID.rs1 = (IF.code >> 15) & 0x1F;
                ID.ext_imm = ext_I(IF.code);
                ID.regrs1 = reg[ID.rs1];
                // 上两条指令检验
                if(ID.rs1 == MEM.rd && ID.rs1 != 0 && MEM.opt != NOOP) {
                    ID.regrs1 = MEM.ALU;
                }
                // 上一条指令检验
                if(ID.rs1 == EX.rd && ID.rs1 != 0 && lastOpt != NOOP) {
                    if(EX.opt == LB || EX.opt == LH || EX.opt == LW || EX.opt == LBU || EX.opt == LHU) {
                        wrback();
                        memacc();
                        // 插入空指令
                        EX.opt = NOOP;
                        EX.rd = 0;
                        EX.rs1 = 0;
                        EX.rs2 = 0;
                        EX.pc_jump = 0;
                        EX.curpc = 0;
                        ID.regrs1 = MEM.ALU;
                    } else {
                        ID.regrs1 = EX.ALU;
                    }
                }
                break;
            case 0x33:
                switch(funct3) {
                case 0x0:
                    switch(funct7) {
                    case 0x0:
                        ID.opt = ADD;
                        break;
                    case 0x20:
                        ID.opt = SUB;
                        break;
                    }
                    break;
                case 0x1:
                    ID.opt = SLL;
                    break;
                case 0x2:
                    ID.opt = SLT;
                    break;
                case 0x3:
                    ID.opt = SLTU;
                    break;
                case 0x4:
                    ID.opt = XOR;
                    break;
                case 0x5:
                    switch(funct7) {
                    case 0x0:
                        ID.opt = SRL;
                        break;
                    case 0x20:
                        ID.opt = SRA;
                        break;
                    }
                    break;
                case 0x6:
                    ID.opt = OR;
                    break;
                case 0x7:
                    ID.opt = AND;
                    break;
                }
                ID.rd = (IF.code >> 7) & 0x1F;
                ID.rs1 = (IF.code >> 15) & 0x1F;
                ID.rs2 = (IF.code >> 20) & 0x1F;
                ID.regrs1 = reg[ID.rs1];
                ID.regrs2 = reg[ID.rs2];
                // 上两条指令检验
                if(ID.rs1 == MEM.rd && ID.rs1 != 0 && MEM.opt != NOOP) {
                    ID.regrs1 = MEM.ALU;
                }
                // 上两条指令检验
                if(ID.rs2 == MEM.rd && ID.rs2 != 0 && MEM.opt != NOOP) {
                    ID.regrs2 = MEM.ALU;
                }
                // 上一条指令检验
                if(ID.rs1 == EX.rd && ID.rs1 != 0 && lastOpt != NOOP) {
                    if(EX.opt == LB || EX.opt == LH || EX.opt == LW || EX.opt == LBU || EX.opt == LHU) {
                        wrback();
                        memacc();
                        // 插入空指令
                        EX.opt = NOOP;
                        EX.rd = 0;
                        EX.rs1 = 0;
                        EX.rs2 = 0;
                        EX.pc_jump = 0;
                        EX.curpc = 0;
                        ID.regrs1 = MEM.ALU;
                    } else {
                        ID.regrs1 = EX.ALU;
                    }
                }
                // 上一条指令检验
                if(ID.rs2 == EX.rd && ID.rs2 != 0 && lastOpt != NOOP) {
                    if(EX.opt == LB || EX.opt == LH || EX.opt == LW || EX.opt == LBU || EX.opt == LHU) {
                        wrback();
                        memacc();
                        // 插入空指令
                        EX.opt = NOOP;
                        EX.rd = 0;
                        EX.rs1 = 0;
                        EX.rs2 = 0;
                        EX.pc_jump = 0;
                        EX.curpc = 0;
                        ID.regrs2 = MEM.ALU;
                    } else {
                        ID.regrs2 = EX.ALU;
                    }
                }
                break;
            }
        }
        // EXECUTE
        void execute() {
            EX.opt = ID.opt;
            EX.rd = ID.rd;
            EX.regrs1 = ID.regrs1;
            EX.regrs2 = ID.regrs2;
            EX.curpc = ID.curpc;
            EX.pc_jump = 0;
            EX.bubble = 0;
            if(ID.opt == NOOP) return;
            EX.code = ID.code;
            if(EX.code == 0x0FF00513U) {
                return;
            }
            switch(ID.opt) {
            case LUI:
                EX.ALU = ID.ext_imm;
                EX.pc_jump = 0;
                break;
            case AUIPC:
                EX.ALU = ID.curpc + ID.ext_imm;
                EX.pc_jump = 0;
                break;
            case JAL:
                EX.ALU = ID.curpc + 4;
                EX.pc_jump = 0;
                break;
            case JALR:
                EX.ALU = ID.curpc + 4;
                EX.pc_jump = 0;
                break;
            case BEQ:
                if(EX.regrs1 == EX.regrs2) {
                    EX.pc_jump = 1;
                } else {
                    EX.pc_jump = 0;
                }
                break;
            case BNE:
                if(EX.regrs1 != EX.regrs2) {
                    EX.pc_jump = 1;
                } else {
                    EX.pc_jump = 0;
                }
                break;
            case BLT:
                if(EX.regrs1 < EX.regrs2) {
                    EX.pc_jump = 1;
                } else {
                    EX.pc_jump = 0;
                }
                break;
            case BGE:
                if(EX.regrs1 >= EX.regrs2) {
                    EX.pc_jump = 1;
                } else {
                    EX.pc_jump = 0;
                }
                break;
            case BLTU:
                if(uint32_t(EX.regrs1) < uint32_t(EX.regrs2)) {
                    EX.pc_jump = 1;
                } else {
                    EX.pc_jump = 0;
                }
                break;
            case BGEU:
                if(uint32_t(EX.regrs1) >= uint32_t(EX.regrs2)) {
                    EX.pc_jump = 1;
                } else {
                    EX.pc_jump = 0;
                }
                break;
            case LB:
                EX.ALU = EX.regrs1 + ID.ext_imm;
                break;
            case LBU:
                EX.ALU = EX.regrs1 + ID.ext_imm;
                break;
            case LH:
                EX.ALU = EX.regrs1 + ID.ext_imm;
                break;
            case LHU:
                EX.ALU = EX.regrs1 + ID.ext_imm;
                break;
            case LW:
                EX.ALU = EX.regrs1 + ID.ext_imm;
                break;
            case SB:
                EX.ALU = EX.regrs1 + ID.ext_imm;
                break;
            case SH:
                EX.ALU = EX.regrs1 + ID.ext_imm;
                break;
            case SW:
                EX.ALU = EX.regrs1 + ID.ext_imm;
                break;
            case ADDI:
                EX.ALU = EX.regrs1 + ID.ext_imm;
                break;
            case SLTI:
                EX.ALU = EX.regrs1 < ID.ext_imm;
                break;
            case SLTIU:
                EX.ALU = uint32_t(EX.regrs1) < uint32_t(ID.ext_imm);
                break;
            case XORI:
                EX.ALU = EX.regrs1 ^ ID.ext_imm;
                break;
            case ORI:
                EX.ALU = EX.regrs1 | ID.ext_imm;
                break;
            case ANDI:
                EX.ALU = EX.regrs1 & ID.ext_imm;
                break;
            case SLLI:
                EX.ALU = EX.regrs1 << ID.ext_imm;
                break;
            case SRLI:
                EX.ALU = uint32_t(EX.regrs1) >> uint32_t(ID.ext_imm);
                break;
            case SRAI:
                EX.ALU = EX.regrs1 >> ID.ext_imm;
                break;
            case ADD:
                EX.ALU = EX.regrs1 + EX.regrs2;
                break;
            case SUB:
                EX.ALU = EX.regrs1 - EX.regrs2;
                break;
            case SLL:
                EX.ALU = EX.regrs1 << (EX.regrs2 & 0x1F);
                break;
            case SLT:
                EX.ALU = EX.regrs1 < EX.regrs2;
                break;
            case SLTU:
                EX.ALU = uint32_t(EX.regrs1) < uint32_t(EX.regrs2);
                break;
            case XOR:
                EX.ALU = EX.regrs1 ^ EX.regrs2;
                break;
            case SRL:
                EX.ALU = uint32_t(EX.regrs1) >> uint32_t(EX.regrs2 & 0x1F);
                break;
            case SRA:
                EX.ALU = EX.regrs1 >> (EX.regrs2 & 0x1F);
                break;
            case OR:
                EX.ALU = EX.regrs1 | EX.regrs2;
                break;
            case AND:
                EX.ALU = EX.regrs1 | EX.regrs2;
                break;
            }
            if(EX.pc_jump) {
                ++ tot_pre;
                if(ID.jump == 0) {
                    // 更新预测器
                    if(bicounter[(EX.curpc >> 2) & 0xff] == 0) {
                        bicounter[(EX.curpc >> 2) & 0xff] = 1;
                    } else {
                        bicounter[(EX.curpc >> 2) & 0xff] = 2;
                    }
                    // 插入空指令
                    ID.opt = NOOP;
                    ID.rd = 0;
                    ID.rs1 = 0;
                    ID.rs2 = 0;
                    ID.curpc = 0;
                    // 重新取指
                    pc = IF.curpc - 4 + ID.ext_imm;
                    fetch();
                    if(endflag == 1) {
                        endflag = 0;
                    }
                    EX.bubble = 1;
                } else {
                    ++ correct_pre;
                    bicounter[(EX.curpc >> 2) & 0xff] = 3;
                }
            } else {
                if(ID.jump == 1) {
                    ++ tot_pre;
                    // 更新预测器
                    if(bicounter[(EX.curpc >> 2) & 0xff] == 2) {
                        bicounter[(EX.curpc >> 2) & 0xff] = 1;
                    } else {
                        bicounter[(EX.curpc >> 2) & 0xff] = 2;
                    }
                    // 插入空指令
                    ID.opt = NOOP;
                    ID.rd = 0;
                    ID.rs1 = 0;
                    ID.rs2 = 0;
                    ID.curpc = 0;
                    EX.bubble = 1;
                    // 重新取指
                    pc = IF.curpc - ID.ext_imm + 4;
                    fetch();
                    if(endflag == 1) {
                        endflag = 0;
                    }
                } else {
                    if(EX.opt == BEQ || EX.opt == BGE || EX.opt == BGEU || EX.opt == BLT || EX.opt == BLTU || EX.opt == BNE) {
                        bicounter[(EX.curpc >> 2) & 0xff] = 0;
                        ++ correct_pre;
                        ++ tot_pre;
                    }
                }
            }
        }
        // MEMORY ACCESS
        void memacc() {
            MEM.rd = EX.rd;
            MEM.opt = EX.opt;
            MEM.curpc = EX.curpc;
            if(EX.opt == NOOP) return;
            MEM.code = EX.code;
            if(MEM.code == 0x0FF00513U) {
                return;
            }
            switch(EX.opt) {
            case LB: {
                int8_t tmp;
                memcpy(&tmp, mem + EX.ALU, 1);
                MEM.ALU = tmp;
                break;
            }
            case LBU: {
                uint8_t tmp;
                memcpy(&tmp, mem + EX.ALU, 1);
                MEM.ALU = tmp;
                break;
            }
            case LH: {
                int16_t tmp;
                memcpy(&tmp, mem + EX.ALU, 2);
                MEM.ALU = tmp;
                break;
            }
            case LHU: {
                uint16_t tmp;
                memcpy(&tmp, mem + EX.ALU, 2);
                MEM.ALU = tmp;
                break;
            }
            case LW: {
                int32_t tmp;
                memcpy(&tmp, mem + EX.ALU, 4);
                MEM.ALU = tmp;
                break;
            }
            case SB: {
                memcpy(mem + EX.ALU, &EX.regrs2, 1);
                break;
            }
            case SH: {
                memcpy(mem + EX.ALU, &EX.regrs2, 2);
                break;
            }
            case SW: {
                memcpy(mem + EX.ALU, &EX.regrs2, 4);
                break;
            }
            default:
                MEM.ALU = EX.ALU;
            }
        }
        // WRITE BACK
        void wrback() {
            WB.opt = MEM.opt;
            if(MEM.opt == NOOP) return;
            if(MEM.rd != 0) {
                reg[MEM.rd] = MEM.ALU;
            }
        }
        void solve() {
            read();
            for(int i = 0;i < 32;++ i) {
                reg[i] = 0;
                bicounter[i] = 0;
            }
            while(1) {
                wrback();
                if(endflag == 1 && IF.opt == NOOP && ID.opt == NOOP && MEM.opt == NOOP && WB.opt == NOOP) {
                    printf("%d\n", ((unsigned int)reg[10]) & 255u);
                    break;
                }
                memacc();
                execute();
                if(EX.bubble) continue;
                decode();
                fetch();
            }
        }
        void showpre() {
            printf("tot: %d\tcorrect: %d\tcorrect rate: %.3f\n", tot_pre, correct_pre, (double)correct_pre / tot_pre);
        }
    };
};

int main() {
    SIM::simulator sim;
    sim.solve();
    // sim.showpre();
    return 0;
}