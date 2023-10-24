//===-- RISCVRegisterBankInfo.cpp -------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
/// \file
/// This file implements the targeting of the RegisterBankInfo class for RISC-V.
/// \todo This should be generated by TableGen.
//===----------------------------------------------------------------------===//

#include "RISCVRegisterBankInfo.h"
#include "MCTargetDesc/RISCVMCTargetDesc.h"
#include "RISCVSubtarget.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/RegisterBank.h"
#include "llvm/CodeGen/RegisterBankInfo.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"

#define GET_TARGET_REGBANK_IMPL
#include "RISCVGenRegisterBank.inc"

namespace llvm {
namespace RISCV {

RegisterBankInfo::PartialMapping PartMappings[] = {
    {0, 32, GPRRegBank},
    {0, 64, GPRRegBank}
};

enum PartialMappingIdx {
  PMI_GPR32 = 0,
  PMI_GPR64 = 1
};

RegisterBankInfo::ValueMapping ValueMappings[] = {
    // Invalid value mapping.
    {nullptr, 0},
    // Maximum 3 GPR operands; 32 bit.
    {&PartMappings[PMI_GPR32], 1},
    {&PartMappings[PMI_GPR32], 1},
    {&PartMappings[PMI_GPR32], 1},
    // Maximum 3 GPR operands; 64 bit.
    {&PartMappings[PMI_GPR64], 1},
    {&PartMappings[PMI_GPR64], 1},
    {&PartMappings[PMI_GPR64], 1}
};

enum ValueMappingsIdx {
  InvalidIdx = 0,
  GPR32Idx = 1,
  GPR64Idx = 4
};
} // namespace RISCV
} // namespace llvm

using namespace llvm;

RISCVRegisterBankInfo::RISCVRegisterBankInfo(unsigned HwMode)
    : RISCVGenRegisterBankInfo(HwMode) {}

const RegisterBank &
RISCVRegisterBankInfo::getRegBankFromRegClass(const TargetRegisterClass &RC,
                                              LLT Ty) const {
  switch (RC.getID()) {
  default:
    llvm_unreachable("Register class not supported");
  case RISCV::GPRRegClassID:
  case RISCV::GPRF16RegClassID:
  case RISCV::GPRF32RegClassID:
  case RISCV::GPRNoX0RegClassID:
  case RISCV::GPRNoX0X2RegClassID:
  case RISCV::GPRJALRRegClassID:
  case RISCV::GPRTCRegClassID:
  case RISCV::GPRC_and_GPRTCRegClassID:
  case RISCV::GPRCRegClassID:
  case RISCV::GPRC_and_SR07RegClassID:
  case RISCV::SR07RegClassID:
  case RISCV::SPRegClassID:
  case RISCV::GPRX0RegClassID:
    return getRegBank(RISCV::GPRRegBankID);
  case RISCV::FPR64RegClassID:
  case RISCV::FPR16RegClassID:
  case RISCV::FPR32RegClassID:
  case RISCV::FPR64CRegClassID:
  case RISCV::FPR32CRegClassID:
    return getRegBank(RISCV::FPRRegBankID);
  }
}

const RegisterBankInfo::InstructionMapping &
RISCVRegisterBankInfo::getInstrMapping(const MachineInstr &MI) const {
  const unsigned Opc = MI.getOpcode();

  // Try the default logic for non-generic instructions that are either copies
  // or already have some operands assigned to banks.
  if (!isPreISelGenericOpcode(Opc) || Opc == TargetOpcode::G_PHI) {
    const InstructionMapping &Mapping = getInstrMappingImpl(MI);
    if (Mapping.isValid())
      return Mapping;
  }

  unsigned GPRSize = getMaximumSize(RISCV::GPRRegBankID);
  assert((GPRSize == 32 || GPRSize == 64) && "Unexpected GPR size");

  unsigned NumOperands = MI.getNumOperands();
  const ValueMapping *GPRValueMapping =
      &RISCV::ValueMappings[GPRSize == 64 ? RISCV::GPR64Idx : RISCV::GPR32Idx];
  const ValueMapping *OperandsMapping = GPRValueMapping;

  switch (Opc) {
  case TargetOpcode::G_ADD:
  case TargetOpcode::G_SUB:
  case TargetOpcode::G_SHL:
  case TargetOpcode::G_ASHR:
  case TargetOpcode::G_LSHR:
  case TargetOpcode::G_AND:
  case TargetOpcode::G_OR:
  case TargetOpcode::G_XOR:
  case TargetOpcode::G_MUL:
  case TargetOpcode::G_SDIV:
  case TargetOpcode::G_SREM:
  case TargetOpcode::G_SMULH:
  case TargetOpcode::G_UDIV:
  case TargetOpcode::G_UREM:
  case TargetOpcode::G_UMULH:
  case TargetOpcode::G_PTR_ADD:
  case TargetOpcode::G_TRUNC:
  case TargetOpcode::G_ANYEXT:
  case TargetOpcode::G_SEXT:
  case TargetOpcode::G_ZEXT:
  case TargetOpcode::G_LOAD:
  case TargetOpcode::G_ZEXTLOAD:
  case TargetOpcode::G_STORE:
    break;
  case TargetOpcode::G_CONSTANT:
  case TargetOpcode::G_FRAME_INDEX:
  case TargetOpcode::G_GLOBAL_VALUE:
  case TargetOpcode::G_BRCOND:
    OperandsMapping = getOperandsMapping({GPRValueMapping, nullptr});
    break;
  case TargetOpcode::G_BR:
    OperandsMapping = getOperandsMapping({nullptr});
    break;
  case TargetOpcode::G_ICMP:
    OperandsMapping = getOperandsMapping(
        {GPRValueMapping, nullptr, GPRValueMapping, GPRValueMapping});
    break;
  case TargetOpcode::G_SEXT_INREG:
    OperandsMapping =
        getOperandsMapping({GPRValueMapping, GPRValueMapping, nullptr});
    break;
  case TargetOpcode::G_SELECT:
    OperandsMapping = getOperandsMapping(
        {GPRValueMapping, GPRValueMapping, GPRValueMapping, GPRValueMapping});
    break;
  default:
    return getInvalidInstructionMapping();
  }

  return getInstructionMapping(DefaultMappingID, /*Cost=*/1, OperandsMapping,
                               NumOperands);
}
