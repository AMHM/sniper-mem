#ifndef INSTRUCTION_INFO_HPP_
#define INSTRUCTION_INFO_HPP_

#include "fixed_types.h"

extern "C" {
#include <xed-decoded-inst.h>
}

#include <vector>
#include <set>

class Instruction;
class MicroOp;

class InstructionDecoder {
private:
   static void addSrcs(std::set<xed_reg_enum_t> regs, MicroOp *uop);
   static void addDsts(std::set<xed_reg_enum_t> regs, MicroOp *uop);
public:
   static std::vector<MicroOp*> decode(IntPtr address, const xed_decoded_inst_t *ins, Instruction *ins_ptr);
};

#endif /* INSTRUCTION_INFO_HPP_ */
