//===-- RISCVInstructionSelector.cpp -----------------------------*- C++ -*-==//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
/// \file
/// This file implements the targeting of the InstructionSelector class for
/// RISC-V.
/// \todo This should be generated by TableGen.
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/RISCVMatInt.h"
#include "RISCVRegisterBankInfo.h"
#include "RISCVSubtarget.h"
#include "RISCVTargetMachine.h"
#include "llvm/CodeGen/GlobalISel/GIMatchTableExecutorImpl.h"
#include "llvm/CodeGen/GlobalISel/GenericMachineInstrs.h"
#include "llvm/CodeGen/GlobalISel/InstructionSelector.h"
#include "llvm/CodeGen/GlobalISel/MIPatternMatch.h"
#include "llvm/CodeGen/GlobalISel/MachineIRBuilder.h"
#include "llvm/IR/IntrinsicsRISCV.h"
#include "llvm/Support/Debug.h"

#define DEBUG_TYPE "riscv-isel"

using namespace llvm;

#define GET_GLOBALISEL_PREDICATE_BITSET
#include "RISCVGenGlobalISel.inc"
#undef GET_GLOBALISEL_PREDICATE_BITSET

namespace {

class RISCVInstructionSelector : public InstructionSelector {
public:
  RISCVInstructionSelector(const RISCVTargetMachine &TM,
                           const RISCVSubtarget &STI,
                           const RISCVRegisterBankInfo &RBI);

  bool select(MachineInstr &MI) override;
  static const char *getName() { return DEBUG_TYPE; }

private:
  const TargetRegisterClass *
  getRegClassForTypeOnBank(LLT Ty, const RegisterBank &RB) const;

  // tblgen-erated 'select' implementation, used as the initial selector for
  // the patterns that don't require complex C++.
  bool selectImpl(MachineInstr &I, CodeGenCoverage &CoverageInfo) const;

  // A lowering phase that runs before any selection attempts.
  // Returns true if the instruction was modified.
  void preISelLower(MachineInstr &MI, MachineIRBuilder &MIB,
                    MachineRegisterInfo &MRI);

  bool replacePtrWithInt(MachineOperand &Op, MachineIRBuilder &MIB,
                         MachineRegisterInfo &MRI);

  // Custom selection methods
  bool selectCopy(MachineInstr &MI, MachineRegisterInfo &MRI) const;
  bool selectConstant(MachineInstr &MI, MachineIRBuilder &MIB,
                      MachineRegisterInfo &MRI) const;
  bool selectSExtInreg(MachineInstr &MI, MachineIRBuilder &MIB) const;
  bool selectSelect(MachineInstr &MI, MachineIRBuilder &MIB,
                    MachineRegisterInfo &MRI) const;

  ComplexRendererFns selectShiftMask(MachineOperand &Root) const;
  ComplexRendererFns selectAddrRegImm(MachineOperand &Root) const;

  ComplexRendererFns selectSHXADDOp(MachineOperand &Root, unsigned ShAmt) const;
  template <unsigned ShAmt>
  ComplexRendererFns selectSHXADDOp(MachineOperand &Root) const {
    return selectSHXADDOp(Root, ShAmt);
  }

  // Custom renderers for tablegen
  void renderNegImm(MachineInstrBuilder &MIB, const MachineInstr &MI,
                    int OpIdx) const;
  void renderImmPlus1(MachineInstrBuilder &MIB, const MachineInstr &MI,
                      int OpIdx) const;
  void renderImm(MachineInstrBuilder &MIB, const MachineInstr &MI,
                 int OpIdx) const;

  const RISCVSubtarget &STI;
  const RISCVInstrInfo &TII;
  const RISCVRegisterInfo &TRI;
  const RISCVRegisterBankInfo &RBI;

  // FIXME: This is necessary because DAGISel uses "Subtarget->" and GlobalISel
  // uses "STI." in the code generated by TableGen. We need to unify the name of
  // Subtarget variable.
  const RISCVSubtarget *Subtarget = &STI;

#define GET_GLOBALISEL_PREDICATES_DECL
#include "RISCVGenGlobalISel.inc"
#undef GET_GLOBALISEL_PREDICATES_DECL

#define GET_GLOBALISEL_TEMPORARIES_DECL
#include "RISCVGenGlobalISel.inc"
#undef GET_GLOBALISEL_TEMPORARIES_DECL
};

} // end anonymous namespace

#define GET_GLOBALISEL_IMPL
#include "RISCVGenGlobalISel.inc"
#undef GET_GLOBALISEL_IMPL

RISCVInstructionSelector::RISCVInstructionSelector(
    const RISCVTargetMachine &TM, const RISCVSubtarget &STI,
    const RISCVRegisterBankInfo &RBI)
    : STI(STI), TII(*STI.getInstrInfo()), TRI(*STI.getRegisterInfo()), RBI(RBI),

#define GET_GLOBALISEL_PREDICATES_INIT
#include "RISCVGenGlobalISel.inc"
#undef GET_GLOBALISEL_PREDICATES_INIT
#define GET_GLOBALISEL_TEMPORARIES_INIT
#include "RISCVGenGlobalISel.inc"
#undef GET_GLOBALISEL_TEMPORARIES_INIT
{
}

InstructionSelector::ComplexRendererFns
RISCVInstructionSelector::selectShiftMask(MachineOperand &Root) const {
  // TODO: Also check if we are seeing the result of an AND operation which
  // could be bypassed since we only check the lower log2(xlen) bits.
  return {{[=](MachineInstrBuilder &MIB) { MIB.add(Root); }}};
}

InstructionSelector::ComplexRendererFns
RISCVInstructionSelector::selectSHXADDOp(MachineOperand &Root,
                                         unsigned ShAmt) const {
  using namespace llvm::MIPatternMatch;
  MachineFunction &MF = *Root.getParent()->getParent()->getParent();
  MachineRegisterInfo &MRI = MF.getRegInfo();

  if (!Root.isReg())
    return std::nullopt;
  Register RootReg = Root.getReg();

  const unsigned XLen = STI.getXLen();
  APInt Mask, C2;
  Register RegY;
  std::optional<bool> LeftShift;
  // (and (shl y, c2), mask)
  if (mi_match(RootReg, MRI,
               m_GAnd(m_GShl(m_Reg(RegY), m_ICst(C2)), m_ICst(Mask))))
    LeftShift = true;
  // (and (lshr y, c2), mask)
  else if (mi_match(RootReg, MRI,
                    m_GAnd(m_GLShr(m_Reg(RegY), m_ICst(C2)), m_ICst(Mask))))
    LeftShift = false;

  if (LeftShift.has_value()) {
    if (*LeftShift)
      Mask &= maskTrailingZeros<uint64_t>(C2.getLimitedValue());
    else
      Mask &= maskTrailingOnes<uint64_t>(XLen - C2.getLimitedValue());

    if (Mask.isShiftedMask()) {
      unsigned Leading = XLen - Mask.getActiveBits();
      unsigned Trailing = Mask.countr_zero();
      // Given (and (shl y, c2), mask) in which mask has no leading zeros and
      // c3 trailing zeros. We can use an SRLI by c3 - c2 followed by a SHXADD.
      if (*LeftShift && Leading == 0 && C2.ult(Trailing) && Trailing == ShAmt) {
        Register DstReg =
            MRI.createGenericVirtualRegister(MRI.getType(RootReg));
        return {{[=](MachineInstrBuilder &MIB) {
          MachineIRBuilder(*MIB.getInstr())
              .buildInstr(RISCV::SRLI, {DstReg}, {RegY})
              .addImm(Trailing - C2.getLimitedValue());
          MIB.addReg(DstReg);
        }}};
      }

      // Given (and (lshr y, c2), mask) in which mask has c2 leading zeros and
      // c3 trailing zeros. We can use an SRLI by c2 + c3 followed by a SHXADD.
      if (!*LeftShift && Leading == C2 && Trailing == ShAmt) {
        Register DstReg =
            MRI.createGenericVirtualRegister(MRI.getType(RootReg));
        return {{[=](MachineInstrBuilder &MIB) {
          MachineIRBuilder(*MIB.getInstr())
              .buildInstr(RISCV::SRLI, {DstReg}, {RegY})
              .addImm(Leading + Trailing);
          MIB.addReg(DstReg);
        }}};
      }
    }
  }

  LeftShift.reset();

  // (shl (and y, mask), c2)
  if (mi_match(RootReg, MRI,
               m_GShl(m_OneNonDBGUse(m_GAnd(m_Reg(RegY), m_ICst(Mask))),
                      m_ICst(C2))))
    LeftShift = true;
  // (lshr (and y, mask), c2)
  else if (mi_match(RootReg, MRI,
                    m_GLShr(m_OneNonDBGUse(m_GAnd(m_Reg(RegY), m_ICst(Mask))),
                            m_ICst(C2))))
    LeftShift = false;

  if (LeftShift.has_value() && Mask.isShiftedMask()) {
    unsigned Leading = XLen - Mask.getActiveBits();
    unsigned Trailing = Mask.countr_zero();

    // Given (shl (and y, mask), c2) in which mask has 32 leading zeros and
    // c3 trailing zeros. If c1 + c3 == ShAmt, we can emit SRLIW + SHXADD.
    bool Cond = *LeftShift && Leading == 32 && Trailing > 0 &&
                (Trailing + C2.getLimitedValue()) == ShAmt;
    if (!Cond)
      // Given (lshr (and y, mask), c2) in which mask has 32 leading zeros and
      // c3 trailing zeros. If c3 - c1 == ShAmt, we can emit SRLIW + SHXADD.
      Cond = !*LeftShift && Leading == 32 && C2.ult(Trailing) &&
             (Trailing - C2.getLimitedValue()) == ShAmt;

    if (Cond) {
      Register DstReg = MRI.createGenericVirtualRegister(MRI.getType(RootReg));
      return {{[=](MachineInstrBuilder &MIB) {
        MachineIRBuilder(*MIB.getInstr())
            .buildInstr(RISCV::SRLIW, {DstReg}, {RegY})
            .addImm(Trailing);
        MIB.addReg(DstReg);
      }}};
    }
  }

  return std::nullopt;
}

InstructionSelector::ComplexRendererFns
RISCVInstructionSelector::selectAddrRegImm(MachineOperand &Root) const {
  // TODO: Need to get the immediate from a G_PTR_ADD. Should this be done in
  // the combiner?
  return {{[=](MachineInstrBuilder &MIB) { MIB.addReg(Root.getReg()); },
           [=](MachineInstrBuilder &MIB) { MIB.addImm(0); }}};
}

bool RISCVInstructionSelector::select(MachineInstr &MI) {
  MachineBasicBlock &MBB = *MI.getParent();
  MachineFunction &MF = *MBB.getParent();
  MachineRegisterInfo &MRI = MF.getRegInfo();
  MachineIRBuilder MIB(MI);

  preISelLower(MI, MIB, MRI);
  const unsigned Opc = MI.getOpcode();

  if (!isPreISelGenericOpcode(Opc) || Opc == TargetOpcode::G_PHI) {
    if (Opc == TargetOpcode::PHI || Opc == TargetOpcode::G_PHI) {
      const Register DefReg = MI.getOperand(0).getReg();
      const LLT DefTy = MRI.getType(DefReg);

      const RegClassOrRegBank &RegClassOrBank =
          MRI.getRegClassOrRegBank(DefReg);

      const TargetRegisterClass *DefRC =
          RegClassOrBank.dyn_cast<const TargetRegisterClass *>();
      if (!DefRC) {
        if (!DefTy.isValid()) {
          LLVM_DEBUG(dbgs() << "PHI operand has no type, not a gvreg?\n");
          return false;
        }

        const RegisterBank &RB = *RegClassOrBank.get<const RegisterBank *>();
        DefRC = getRegClassForTypeOnBank(DefTy, RB);
        if (!DefRC) {
          LLVM_DEBUG(dbgs() << "PHI operand has unexpected size/bank\n");
          return false;
        }
      }

      MI.setDesc(TII.get(TargetOpcode::PHI));
      return RBI.constrainGenericRegister(DefReg, *DefRC, MRI);
    }

    // Certain non-generic instructions also need some special handling.
    if (MI.isCopy())
      return selectCopy(MI, MRI);

    return true;
  }

  if (selectImpl(MI, *CoverageInfo))
    return true;

  switch (Opc) {
  case TargetOpcode::G_ANYEXT:
  case TargetOpcode::G_PTRTOINT:
  case TargetOpcode::G_TRUNC:
    return selectCopy(MI, MRI);
  case TargetOpcode::G_CONSTANT:
    return selectConstant(MI, MIB, MRI);
  case TargetOpcode::G_BRCOND: {
    // TODO: Fold with G_ICMP.
    auto Bcc =
        MIB.buildInstr(RISCV::BNE, {}, {MI.getOperand(0), Register(RISCV::X0)})
            .addMBB(MI.getOperand(1).getMBB());
    MI.eraseFromParent();
    return constrainSelectedInstRegOperands(*Bcc, TII, TRI, RBI);
  }
  case TargetOpcode::G_SEXT_INREG:
    return selectSExtInreg(MI, MIB);
  case TargetOpcode::G_FRAME_INDEX: {
    // TODO: We may want to replace this code with the SelectionDAG patterns,
    // which fail to get imported because it uses FrameAddrRegImm, which is a
    // ComplexPattern
    MI.setDesc(TII.get(RISCV::ADDI));
    MI.addOperand(MachineOperand::CreateImm(0));
    return constrainSelectedInstRegOperands(MI, TII, TRI, RBI);
  }
  case TargetOpcode::G_SELECT:
    return selectSelect(MI, MIB, MRI);
  default:
    return false;
  }
}

bool RISCVInstructionSelector::replacePtrWithInt(MachineOperand &Op,
                                                 MachineIRBuilder &MIB,
                                                 MachineRegisterInfo &MRI) {
  Register PtrReg = Op.getReg();
  assert(MRI.getType(PtrReg).isPointer() && "Operand is not a pointer!");

  const LLT XLenLLT = LLT::scalar(STI.getXLen());
  auto PtrToInt = MIB.buildPtrToInt(XLenLLT, PtrReg);
  MRI.setRegBank(PtrToInt.getReg(0), RBI.getRegBank(RISCV::GPRRegBankID));
  Op.setReg(PtrToInt.getReg(0));
  return select(*PtrToInt);
}

void RISCVInstructionSelector::preISelLower(MachineInstr &MI,
                                            MachineIRBuilder &MIB,
                                            MachineRegisterInfo &MRI) {
  switch (MI.getOpcode()) {
  case TargetOpcode::G_PTR_ADD: {
    Register DstReg = MI.getOperand(0).getReg();
    const LLT XLenLLT = LLT::scalar(STI.getXLen());

    replacePtrWithInt(MI.getOperand(1), MIB, MRI);
    MI.setDesc(TII.get(TargetOpcode::G_ADD));
    MRI.setType(DstReg, XLenLLT);
    break;
  }
  }
}

void RISCVInstructionSelector::renderNegImm(MachineInstrBuilder &MIB,
                                            const MachineInstr &MI,
                                            int OpIdx) const {
  assert(MI.getOpcode() == TargetOpcode::G_CONSTANT && OpIdx == -1 &&
         "Expected G_CONSTANT");
  int64_t CstVal = MI.getOperand(1).getCImm()->getSExtValue();
  MIB.addImm(-CstVal);
}

void RISCVInstructionSelector::renderImmPlus1(MachineInstrBuilder &MIB,
                                              const MachineInstr &MI,
                                              int OpIdx) const {
  assert(MI.getOpcode() == TargetOpcode::G_CONSTANT && OpIdx == -1 &&
         "Expected G_CONSTANT");
  int64_t CstVal = MI.getOperand(1).getCImm()->getSExtValue();
  MIB.addImm(CstVal + 1);
}

void RISCVInstructionSelector::renderImm(MachineInstrBuilder &MIB,
                                         const MachineInstr &MI,
                                         int OpIdx) const {
  assert(MI.getOpcode() == TargetOpcode::G_CONSTANT && OpIdx == -1 &&
         "Expected G_CONSTANT");
  int64_t CstVal = MI.getOperand(1).getCImm()->getSExtValue();
  MIB.addImm(CstVal);
}

const TargetRegisterClass *RISCVInstructionSelector::getRegClassForTypeOnBank(
    LLT Ty, const RegisterBank &RB) const {
  if (RB.getID() == RISCV::GPRRegBankID) {
    if (Ty.getSizeInBits() <= 32 || (STI.is64Bit() && Ty.getSizeInBits() == 64))
      return &RISCV::GPRRegClass;
  }

  // TODO: Non-GPR register classes.
  return nullptr;
}

bool RISCVInstructionSelector::selectCopy(MachineInstr &MI,
                                          MachineRegisterInfo &MRI) const {
  Register DstReg = MI.getOperand(0).getReg();

  if (DstReg.isPhysical())
    return true;

  const TargetRegisterClass *DstRC = getRegClassForTypeOnBank(
      MRI.getType(DstReg), *RBI.getRegBank(DstReg, MRI, TRI));
  assert(DstRC &&
         "Register class not available for LLT, register bank combination");

  // No need to constrain SrcReg. It will get constrained when
  // we hit another of its uses or its defs.
  // Copies do not have constraints.
  if (!RBI.constrainGenericRegister(DstReg, *DstRC, MRI)) {
    LLVM_DEBUG(dbgs() << "Failed to constrain " << TII.getName(MI.getOpcode())
                      << " operand\n");
    return false;
  }

  MI.setDesc(TII.get(RISCV::COPY));
  return true;
}

bool RISCVInstructionSelector::selectConstant(MachineInstr &MI,
                                              MachineIRBuilder &MIB,
                                              MachineRegisterInfo &MRI) const {
  assert(MI.getOpcode() == TargetOpcode::G_CONSTANT);
  Register FinalReg = MI.getOperand(0).getReg();
  int64_t Imm = MI.getOperand(1).getCImm()->getSExtValue();

  if (Imm == 0) {
    MI.getOperand(1).ChangeToRegister(RISCV::X0, false);
    RBI.constrainGenericRegister(FinalReg, RISCV::GPRRegClass, MRI);
    MI.setDesc(TII.get(TargetOpcode::COPY));
    return true;
  }

  RISCVMatInt::InstSeq Seq =
      RISCVMatInt::generateInstSeq(Imm, Subtarget->getFeatureBits());
  unsigned NumInsts = Seq.size();
  Register SrcReg = RISCV::X0;

  for (unsigned i = 0; i < NumInsts; i++) {
    Register DstReg = i < NumInsts - 1
                          ? MRI.createVirtualRegister(&RISCV::GPRRegClass)
                          : FinalReg;
    const RISCVMatInt::Inst &I = Seq[i];
    MachineInstr *Result;

    switch (I.getOpndKind()) {
    case RISCVMatInt::Imm:
      // clang-format off
      Result = MIB.buildInstr(I.getOpcode())
                   .addDef(DstReg)
                   .addImm(I.getImm());
      // clang-format on
      break;
    case RISCVMatInt::RegX0:
      Result = MIB.buildInstr(I.getOpcode())
                   .addDef(DstReg)
                   .addReg(SrcReg)
                   .addReg(RISCV::X0);
      break;
    case RISCVMatInt::RegReg:
      Result = MIB.buildInstr(I.getOpcode())
                   .addDef(DstReg)
                   .addReg(SrcReg)
                   .addReg(SrcReg);
      break;
    case RISCVMatInt::RegImm:
      Result = MIB.buildInstr(I.getOpcode())
                   .addDef(DstReg)
                   .addReg(SrcReg)
                   .addImm(I.getImm());
      break;
    }

    if (!constrainSelectedInstRegOperands(*Result, TII, TRI, RBI))
      return false;

    SrcReg = DstReg;
  }

  MI.eraseFromParent();
  return true;
}

bool RISCVInstructionSelector::selectSExtInreg(MachineInstr &MI,
                                               MachineIRBuilder &MIB) const {
  if (!STI.isRV64())
    return false;

  const MachineOperand &Size = MI.getOperand(2);
  // Only Size == 32 (i.e. shift by 32 bits) is acceptable at this point.
  if (!Size.isImm() || Size.getImm() != 32)
    return false;

  const MachineOperand &Src = MI.getOperand(1);
  const MachineOperand &Dst = MI.getOperand(0);
  // addiw rd, rs, 0 (i.e. sext.w rd, rs)
  MachineInstr *NewMI =
      MIB.buildInstr(RISCV::ADDIW, {Dst.getReg()}, {Src.getReg()}).addImm(0U);

  if (!constrainSelectedInstRegOperands(*NewMI, TII, TRI, RBI))
    return false;

  MI.eraseFromParent();
  return true;
}

bool RISCVInstructionSelector::selectSelect(MachineInstr &MI,
                                            MachineIRBuilder &MIB,
                                            MachineRegisterInfo &MRI) const {
  // TODO: Currently we check that the conditional code passed to G_SELECT is
  // not equal to zero; however, in the future, we might want to try and check
  // if the conditional code comes from a G_ICMP. If it does, we can directly
  // use G_ICMP to get the first three input operands of the
  // Select_GPR_Using_CC_GPR. This might be done here, or in the appropriate
  // combiner.
  assert(MI.getOpcode() == TargetOpcode::G_SELECT);
  MachineInstr *Result = MIB.buildInstr(RISCV::Select_GPR_Using_CC_GPR)
                             .addDef(MI.getOperand(0).getReg())
                             .addReg(MI.getOperand(1).getReg())
                             .addReg(RISCV::X0)
                             .addImm(RISCVCC::COND_NE)
                             .addReg(MI.getOperand(2).getReg())
                             .addReg(MI.getOperand(3).getReg());
  MI.eraseFromParent();
  return constrainSelectedInstRegOperands(*Result, TII, TRI, RBI);
}

namespace llvm {
InstructionSelector *
createRISCVInstructionSelector(const RISCVTargetMachine &TM,
                               RISCVSubtarget &Subtarget,
                               RISCVRegisterBankInfo &RBI) {
  return new RISCVInstructionSelector(TM, Subtarget, RBI);
}
} // end namespace llvm
