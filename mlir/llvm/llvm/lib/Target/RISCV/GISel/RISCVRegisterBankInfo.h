//===-- RISCVRegisterBankInfo.h ---------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
/// \file
/// This file declares the targeting of the RegisterBankInfo class for RISC-V.
/// \todo This should be generated by TableGen.
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_RISCV_RISCVREGISTERBANKINFO_H
#define LLVM_LIB_TARGET_RISCV_RISCVREGISTERBANKINFO_H

#include "llvm/CodeGen/RegisterBankInfo.h"

#define GET_REGBANK_DECLARATIONS
#include "RISCVGenRegisterBank.inc"

namespace llvm {

class TargetRegisterInfo;

class RISCVGenRegisterBankInfo : public RegisterBankInfo {
protected:
#define GET_TARGET_REGBANK_CLASS
#include "RISCVGenRegisterBank.inc"
};

/// This class provides the information for the target register banks.
class RISCVRegisterBankInfo final : public RISCVGenRegisterBankInfo {
public:
  RISCVRegisterBankInfo(unsigned HwMode);

  const RegisterBank &getRegBankFromRegClass(const TargetRegisterClass &RC,
                                             LLT Ty) const override;

  const InstructionMapping &
  getInstrMapping(const MachineInstr &MI) const override;
};
} // end namespace llvm
#endif
