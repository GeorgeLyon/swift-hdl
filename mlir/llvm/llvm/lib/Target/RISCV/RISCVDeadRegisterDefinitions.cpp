//===- RISCVDeadRegisterDefinitions.cpp - Replace dead defs w/ zero reg --===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===---------------------------------------------------------------------===//
//
// This pass rewrites Rd to x0 for instrs whose return values are unused.
//
//===---------------------------------------------------------------------===//

#include "RISCV.h"
#include "RISCVInstrInfo.h"
#include "RISCVSubtarget.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"

using namespace llvm;
#define DEBUG_TYPE "riscv-dead-defs"
#define RISCV_DEAD_REG_DEF_NAME "RISC-V Dead register definitions"

STATISTIC(NumDeadDefsReplaced, "Number of dead definitions replaced");

namespace {
class RISCVDeadRegisterDefinitions : public MachineFunctionPass {
public:
  static char ID;

  RISCVDeadRegisterDefinitions() : MachineFunctionPass(ID) {
    initializeRISCVDeadRegisterDefinitionsPass(
        *PassRegistry::getPassRegistry());
  }
  bool runOnMachineFunction(MachineFunction &MF) override;
  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesCFG();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

  StringRef getPassName() const override { return RISCV_DEAD_REG_DEF_NAME; }
};
} // end anonymous namespace

char RISCVDeadRegisterDefinitions::ID = 0;
INITIALIZE_PASS(RISCVDeadRegisterDefinitions, DEBUG_TYPE,
                RISCV_DEAD_REG_DEF_NAME, false, false)

FunctionPass *llvm::createRISCVDeadRegisterDefinitionsPass() {
  return new RISCVDeadRegisterDefinitions();
}

bool RISCVDeadRegisterDefinitions::runOnMachineFunction(MachineFunction &MF) {
  if (skipFunction(MF.getFunction()))
    return false;

  const MachineRegisterInfo *MRI = &MF.getRegInfo();
  const TargetInstrInfo *TII = MF.getSubtarget().getInstrInfo();
  const TargetRegisterInfo *TRI = MF.getSubtarget().getRegisterInfo();
  LLVM_DEBUG(dbgs() << "***** RISCVDeadRegisterDefinitions *****\n");

  bool MadeChange = false;
  for (MachineBasicBlock &MBB : MF) {
    for (MachineInstr &MI : MBB) {
      // We only handle non-computational instructions since some NOP encodings
      // are reserved for HINT instructions.
      const MCInstrDesc &Desc = MI.getDesc();
      if (!Desc.mayLoad() && !Desc.mayStore() &&
          !Desc.hasUnmodeledSideEffects())
        continue;
      // For PseudoVSETVLIX0, Rd = X0 has special meaning.
      if (MI.getOpcode() == RISCV::PseudoVSETVLIX0)
        continue;
      for (int I = 0, E = Desc.getNumDefs(); I != E; ++I) {
        MachineOperand &MO = MI.getOperand(I);
        if (!MO.isReg() || !MO.isDef() || MO.isEarlyClobber())
          continue;
        // Be careful not to change the register if it's a tied operand.
        if (MI.isRegTiedToUseOperand(I)) {
          LLVM_DEBUG(dbgs() << "    Ignoring, def is tied operand.\n");
          continue;
        }
        // We should not have any relevant physreg defs that are replacable by
        // zero before register allocation. So we just check for dead vreg defs.
        Register Reg = MO.getReg();
        if (!Reg.isVirtual() || (!MO.isDead() && !MRI->use_nodbg_empty(Reg)))
          continue;
        LLVM_DEBUG(dbgs() << "    Dead def operand #" << I << " in:\n      ";
                   MI.print(dbgs()));
        const TargetRegisterClass *RC = TII->getRegClass(Desc, I, TRI, MF);
        if (!(RC && RC->contains(RISCV::X0))) {
          LLVM_DEBUG(dbgs() << "    Ignoring, register is not a GPR.\n");
          continue;
        }
        MO.setReg(RISCV::X0);
        MO.setIsDead();
        LLVM_DEBUG(dbgs() << "    Replacing with zero register. New:\n      ";
                   MI.print(dbgs()));
        ++NumDeadDefsReplaced;
        MadeChange = true;
      }
    }
  }

  return MadeChange;
}
