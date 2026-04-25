#include "blocktype/IR/ADT/APInt.h"
#include "blocktype/IR/ADT/raw_ostream.h"
#include "blocktype/IR/ADT/StringRef.h"

#include <algorithm>

namespace blocktype {
namespace ir {

APInt knuthDivImpl(const APInt& LHS, const APInt& RHS, bool ComputeRemainder) {
  unsigned BW = LHS.getBitWidth();
  assert(BW == RHS.getBitWidth() && "bitwidth mismatch");
  assert(!RHS.isZero() && "division by zero");

  if (LHS.isZero()) return APInt(BW, uint64_t(0));

  if (BW <= 64) {
    uint64_t A = LHS.getZExtValue();
    uint64_t B = RHS.getZExtValue();
    return APInt(BW, ComputeRemainder ? (A % B) : (A / B));
  }

  unsigned N = LHS.getNumWords();
  unsigned M = RHS.getNumWords();

  if (N < M) {
    return ComputeRemainder ? LHS : APInt(BW, uint64_t(0));
  }

  if (N == M) {
    bool LHSLarger = false;
    for (int i = static_cast<int>(N) - 1; i >= 0; --i) {
      uint64_t A = LHS.getRawData(i);
      uint64_t B = RHS.getRawData(i);
      if (A > B) { LHSLarger = true; break; }
      if (A < B) { LHSLarger = false; break; }
    }
    if (!LHSLarger) {
      return ComputeRemainder ? LHS : APInt(BW, uint64_t(0));
    }
  }

  std::vector<uint64_t> Dividend(N);
  for (unsigned i = 0; i < N; ++i) Dividend[i] = LHS.getRawData(i);

  std::vector<uint64_t> Divisor(M);
  for (unsigned i = 0; i < M; ++i) Divisor[i] = RHS.getRawData(i);

  unsigned Shift = static_cast<unsigned>(__builtin_clzll(Divisor[M - 1]));
  uint64_t Carry = 0;
  for (int i = static_cast<int>(N) - 1; i >= 0; --i) {
    uint64_t W = Dividend[i];
    Dividend[i] = (W << Shift) | Carry;
    Carry = W >> (64 - Shift);
  }
  Carry = 0;
  for (int i = static_cast<int>(M) - 1; i >= 0; --i) {
    uint64_t W = Divisor[i];
    Divisor[i] = (W << Shift) | Carry;
    Carry = W >> (64 - Shift);
  }

  std::vector<uint64_t> Quotient(N - M + 1, 0);
  std::vector<uint64_t> Remainder(M, 0);

  for (int j = static_cast<int>(N - M); j >= 0; --j) {
    __uint128_t DivHi = static_cast<__uint128_t>(Dividend[j + M]) * 2 + Dividend[j + M - 1];
    uint64_t QHat = static_cast<uint64_t>(DivHi / Divisor[M - 1]);
    uint64_t RHat = static_cast<uint64_t>(DivHi % Divisor[M - 1]);

    while (QHat >= (1ULL << 63) ||
           QHat * (M >= 2 ? Divisor[M - 2] : 0) > ((static_cast<__uint128_t>(RHat) << 64) + Dividend[j + M - 2])) {
      --QHat;
      RHat += Divisor[M - 1];
      if (RHat >= (1ULL << 63)) break;
    }

    int64_t Carry2 = 0;
    for (unsigned i = 0; i < M; ++i) {
      __uint128_t Prod = static_cast<__uint128_t>(QHat) * Divisor[i];
      int64_t Sub = static_cast<int64_t>(Dividend[j + i]) - static_cast<int64_t>(static_cast<uint64_t>(Prod)) - Carry2;
      Dividend[j + i] = static_cast<uint64_t>(Sub);
      Carry2 = -static_cast<int64_t>(Prod >> 64) - (Sub < 0 ? 1 : 0);
    }
    int64_t Sub = static_cast<int64_t>(Dividend[j + M]) - Carry2;
    Dividend[j + M] = static_cast<uint64_t>(Sub);

    Quotient[j] = QHat;
    if (Sub < 0) {
      --Quotient[j];
      uint64_t Carry3 = 0;
      for (unsigned i = 0; i < M; ++i) {
        uint64_t Sum = Dividend[j + i] + Divisor[i] + Carry3;
        Carry3 = (Sum < Dividend[j + i]) || (Carry3 && Sum == Dividend[j + i]) ? 1 : 0;
        Dividend[j + i] = Sum;
      }
      Dividend[j + M] += Carry3;
    }
  }

  if (ComputeRemainder) {
    uint64_t Carry4 = 0;
    for (unsigned i = 0; i < M; ++i) {
      uint64_t W = Dividend[i];
      Dividend[i] = (W >> Shift) | Carry4;
      Carry4 = W << (64 - Shift);
    }
    APInt Result(BW, uint64_t(0));
    for (unsigned i = 0; i < M; ++i)
      Result.MultiWord[i] = Dividend[i];
    Result.clearUnusedBits();
    return Result;
  }

  APInt Result(BW, uint64_t(0));
  for (unsigned i = 0; i < Quotient.size() && i < Result.getNumWords(); ++i)
    Result.MultiWord[i] = Quotient[i];
  Result.clearUnusedBits();
  return Result;
}

APInt APInt::udiv(const APInt& O) const {
  return knuthDivImpl(*this, O, false);
}

APInt APInt::sdiv(const APInt& O) const {
  assert(BitWidth == O.BitWidth && "bitwidth mismatch");
  if (isNegative()) {
    if (O.isNegative()) {
      return (-*this).udiv(-O);
    }
    return -(-*this).udiv(O);
  }
  if (O.isNegative()) {
    return -(udiv(-O));
  }
  return udiv(O);
}

APInt APInt::urem(const APInt& O) const {
  return knuthDivImpl(*this, O, true);
}

APInt APInt::srem(const APInt& O) const {
  assert(BitWidth == O.BitWidth && "bitwidth mismatch");
  if (isNegative()) {
    if (O.isNegative()) {
      return -(-*this).urem(-O);
    }
    return -(-*this).urem(O);
  }
  if (O.isNegative()) {
    return urem(-O);
  }
  return urem(O);
}

std::string APInt::toString(unsigned Radix, bool IsSigned) const {
  if (BitWidth == 0) return "0";

  if (isSingleWord()) {
    if (Radix == 10 && IsSigned) {
      return std::to_string(getSExtValue());
    }
    if (Radix == 16) {
      char Buf[20];
      snprintf(Buf, sizeof(Buf), "0x%llx", static_cast<unsigned long long>(getZExtValue()));
      return Buf;
    }
    if (Radix == 8) {
      char Buf[24];
      snprintf(Buf, sizeof(Buf), "0o%llo", static_cast<unsigned long long>(getZExtValue()));
      return Buf;
    }
    if (Radix == 2) {
      std::string S;
      for (unsigned i = BitWidth; i > 0; --i)
        S += ((*this)[i - 1]) ? '1' : '0';
      return S;
    }
    return std::to_string(getZExtValue());
  }

  APInt Tmp = *this;
  bool Neg = IsSigned && Tmp.isNegative();
  if (Neg) Tmp = -Tmp;

  std::string Result;
  if (Radix == 10) {
    APInt Zero(BitWidth, uint64_t(0));
    APInt RadixVal(BitWidth, Radix);
    while (!Tmp.isZero()) {
      APInt Rem = Tmp.urem(RadixVal);
      Result += static_cast<char>('0' + Rem.getZExtValue());
      Tmp = Tmp.udiv(RadixVal);
    }
    if (Result.empty()) Result = "0";
    if (Neg) Result += '-';
    std::reverse(Result.begin(), Result.end());
    return Result;
  }

  if (Radix == 16) {
    while (!Tmp.isZero()) {
      APInt Rem = Tmp.urem(APInt(BitWidth, 16));
      unsigned D = static_cast<unsigned>(Rem.getZExtValue());
      Result += (D < 10) ? ('0' + D) : ('a' + D - 10);
      Tmp = Tmp.udiv(APInt(BitWidth, 16));
    }
    if (Result.empty()) Result = "0";
    std::reverse(Result.begin(), Result.end());
    return "0x" + Result;
  }

  return Tmp.toString(10, IsSigned);
}

void APInt::print(raw_ostream& OS) const {
  OS << toString(10, true);
}

} // namespace ir
} // namespace blocktype
