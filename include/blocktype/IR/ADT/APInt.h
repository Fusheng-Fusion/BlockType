#ifndef BLOCKTYPE_IR_ADT_APINT_H
#define BLOCKTYPE_IR_ADT_APINT_H

#include <algorithm>
#include <cassert>
#include <climits>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace blocktype {
namespace ir {

class raw_ostream;
class StringRef;

class APInt {
  friend APInt knuthDivImpl(const APInt&, const APInt&, bool);
  unsigned BitWidth;
  unsigned NumWords;
  uint64_t SingleWord;
  std::vector<uint64_t> MultiWord;

  static unsigned getNumWords(unsigned BW) {
    return (BW + 63) / 64;
  }

  static uint64_t wordMask(unsigned BW, unsigned WordIdx) {
    unsigned HighBit = BW - WordIdx * 64;
    if (HighBit >= 64) return ~uint64_t(0);
    return (uint64_t(1) << HighBit) - 1;
  }

  bool isSingleWord() const { return NumWords == 1; }

  uint64_t* words() {
    return isSingleWord() ? &SingleWord : MultiWord.data();
  }

  const uint64_t* words() const {
    return isSingleWord() ? &SingleWord : MultiWord.data();
  }

  uint64_t word(unsigned i) const {
    if (i == 0 && isSingleWord()) return SingleWord;
    if (i < MultiWord.size()) return MultiWord[i];
    return 0;
  }

  void setWord(unsigned i, uint64_t Val) {
    if (isSingleWord()) { SingleWord = Val; return; }
    MultiWord[i] = Val;
  }

  void clearUnusedBits() {
    unsigned TopBits = BitWidth % 64;
    if (TopBits == 0) return;
    unsigned TopWord = NumWords - 1;
    uint64_t Mask = (uint64_t(1) << TopBits) - 1;
    if (isSingleWord()) {
      SingleWord &= Mask;
    } else {
      MultiWord[TopWord] &= Mask;
    }
  }

  void initFromU64(uint64_t Val) {
    if (isSingleWord()) {
      SingleWord = Val;
    } else {
      MultiWord.resize(NumWords, 0);
      MultiWord[0] = Val;
    }
    clearUnusedBits();
  }

  void initFromSignedU64(uint64_t Val) {
    if (isSingleWord()) {
      SingleWord = Val;
    } else {
      MultiWord.resize(NumWords, 0);
      MultiWord[0] = Val;
      if (static_cast<int64_t>(Val) < 0 && NumWords > 1) {
        for (unsigned i = 1; i < NumWords; ++i)
          MultiWord[i] = ~uint64_t(0);
      }
    }
    clearUnusedBits();
  }

public:
  APInt() : BitWidth(0), NumWords(1), SingleWord(0) {}

  APInt(unsigned BW, uint64_t Val, bool IsSigned = false)
    : BitWidth(BW), NumWords(getNumWords(BW ? BW : 1)), SingleWord(0) {
    if (BW == 0) { SingleWord = 0; return; }
    if (IsSigned) initFromSignedU64(Val);
    else initFromU64(Val);
  }

  APInt(unsigned BW, const uint64_t* RawWords, unsigned NumRaw)
    : BitWidth(BW), NumWords(getNumWords(BW ? BW : 1)), SingleWord(0) {
    if (isSingleWord()) {
      SingleWord = RawWords ? RawWords[0] : 0;
    } else {
      MultiWord.resize(NumWords, 0);
      for (unsigned i = 0; i < std::min(NumRaw, NumWords); ++i)
        MultiWord[i] = RawWords[i];
    }
    clearUnusedBits();
  }

  APInt(const APInt& O)
    : BitWidth(O.BitWidth), NumWords(O.NumWords), SingleWord(O.SingleWord),
      MultiWord(O.MultiWord) {}

  APInt(APInt&& O) noexcept
    : BitWidth(O.BitWidth), NumWords(O.NumWords), SingleWord(O.SingleWord),
      MultiWord(std::move(O.MultiWord)) {
    O.BitWidth = 0;
    O.NumWords = 1;
    O.SingleWord = 0;
  }

  APInt& operator=(const APInt& O) {
    if (this == &O) return *this;
    BitWidth = O.BitWidth;
    NumWords = O.NumWords;
    SingleWord = O.SingleWord;
    MultiWord = O.MultiWord;
    return *this;
  }

  APInt& operator=(APInt&& O) noexcept {
    if (this == &O) return *this;
    BitWidth = O.BitWidth;
    NumWords = O.NumWords;
    SingleWord = O.SingleWord;
    MultiWord = std::move(O.MultiWord);
    O.BitWidth = 0;
    O.NumWords = 1;
    O.SingleWord = 0;
    return *this;
  }

  static APInt getZero(unsigned BW) { return APInt(BW, uint64_t(0)); }
  static APInt getOne(unsigned BW) { return APInt(BW, uint64_t(1)); }
  static APInt getAllOnes(unsigned BW) { return APInt(BW, ~uint64_t(0), true); }

  static APInt getSignedMaxValue(unsigned BW) {
    APInt R(BW, ~uint64_t(0));
    R.clearBit(BW - 1);
    return R;
  }

  static APInt getSignedMinValue(unsigned BW) {
    APInt R(BW, uint64_t(0));
    R.setBit(BW - 1);
    return R;
  }

  unsigned getBitWidth() const { return BitWidth; }
  unsigned getNumWords() const { return NumWords; }

  uint64_t getZExtValue() const {
    if (BitWidth == 0) return 0;
    return word(0);
  }

  int64_t getSExtValue() const {
    if (BitWidth == 0) return 0;
    if (BitWidth <= 64) {
      uint64_t V = word(0);
      if ((V >> (BitWidth - 1)) & 1) {
        uint64_t Mask = ~((uint64_t(1) << BitWidth) - 1);
        return static_cast<int64_t>(V | Mask);
      }
      return static_cast<int64_t>(V);
    }
    if (word(NumWords - 1) >> 63) {
      return static_cast<int64_t>(~uint64_t(0));
    }
    return static_cast<int64_t>(word(0));
  }

  uint64_t getRawData(unsigned WordIdx) const {
    return word(WordIdx);
  }

  bool isZero() const {
    if (isSingleWord()) return SingleWord == 0;
    for (unsigned i = 0; i < NumWords; ++i)
      if (MultiWord[i] != 0) return false;
    return true;
  }

  bool isAllOnesValue() const {
    if (BitWidth == 0) return true;
    if (isSingleWord()) {
      uint64_t Mask = BitWidth == 64 ? ~uint64_t(0) : ((uint64_t(1) << BitWidth) - 1);
      return (SingleWord & Mask) == Mask;
    }
    unsigned FullWords = BitWidth / 64;
    for (unsigned i = 0; i < FullWords; ++i)
      if (word(i) != ~uint64_t(0)) return false;
    unsigned RemBits = BitWidth % 64;
    if (RemBits > 0) {
      uint64_t Mask = (uint64_t(1) << RemBits) - 1;
      if (word(FullWords) != Mask) return false;
    }
    return true;
  }

  bool isNegative() const {
    return BitWidth > 0 && (*this)[BitWidth - 1];
  }

  bool isStrictlyPositive() const {
    return !isNegative() && !isZero();
  }

  bool operator[](unsigned BitIdx) const {
    assert(BitIdx < BitWidth && "Bit index out of range");
    return (word(BitIdx / 64) >> (BitIdx % 64)) & 1;
  }

  void setBit(unsigned BitIdx) {
    assert(BitIdx < BitWidth && "Bit index out of range");
    uint64_t W = word(BitIdx / 64);
    W |= (uint64_t(1) << (BitIdx % 64));
    setWord(BitIdx / 64, W);
  }

  void clearBit(unsigned BitIdx) {
    assert(BitIdx < BitWidth && "Bit index out of range");
    uint64_t W = word(BitIdx / 64);
    W &= ~(uint64_t(1) << (BitIdx % 64));
    setWord(BitIdx / 64, W);
  }

  APInt trunc(unsigned BW) const {
    assert(BW <= BitWidth && "truncate to larger bitwidth");
    if (BW == 0) return APInt(0, uint64_t(0));
    APInt Result(BW, uint64_t(0));
    unsigned CopyWords = std::min(Result.NumWords, NumWords);
    if (Result.isSingleWord()) {
      Result.SingleWord = word(0);
    } else {
      for (unsigned i = 0; i < CopyWords; ++i)
        Result.MultiWord[i] = word(i);
    }
    Result.clearUnusedBits();
    return Result;
  }

  APInt zext(unsigned BW) const {
    assert(BW >= BitWidth && "extend to smaller bitwidth");
    if (BW == BitWidth) return *this;
    APInt Result(BW, uint64_t(0));
    if (Result.isSingleWord()) {
      Result.SingleWord = getZExtValue();
    } else {
      for (unsigned i = 0; i < NumWords; ++i)
        Result.MultiWord[i] = word(i);
    }
    return Result;
  }

  APInt sext(unsigned BW) const {
    assert(BW >= BitWidth && "extend to smaller bitwidth");
    if (BW == BitWidth) return *this;
    if (BitWidth == 0) return APInt(BW, uint64_t(0));
    APInt Result(BW, uint64_t(0));
    if (Result.isSingleWord()) {
      if (isNegative()) {
        uint64_t SignMask = ~((uint64_t(1) << BitWidth) - 1);
        Result.SingleWord = (word(0) | SignMask);
      } else {
        Result.SingleWord = word(0);
      }
      Result.clearUnusedBits();
    } else {
      for (unsigned i = 0; i < std::min(NumWords, Result.NumWords); ++i)
        Result.MultiWord[i] = word(i);
      if (isNegative()) {
        for (unsigned i = NumWords; i < Result.NumWords; ++i)
          Result.MultiWord[i] = ~uint64_t(0);
        if (BitWidth % 64 != 0) {
          unsigned TopSrcWord = NumWords - 1;
          unsigned BitsInTop = BitWidth % 64;
          uint64_t SignMask = ~((uint64_t(1) << BitsInTop) - 1);
          Result.MultiWord[TopSrcWord] |= SignMask;
        }
      }
      Result.clearUnusedBits();
    }
    return Result;
  }

  APInt zextOrTrunc(unsigned BW) const {
    if (BW > BitWidth) return zext(BW);
    if (BW < BitWidth) return trunc(BW);
    return *this;
  }

  APInt sextOrTrunc(unsigned BW) const {
    if (BW > BitWidth) return sext(BW);
    if (BW < BitWidth) return trunc(BW);
    return *this;
  }

  bool operator==(const APInt& O) const {
    assert(BitWidth == O.BitWidth && "bitwidth mismatch");
    for (unsigned i = 0; i < NumWords; ++i)
      if (word(i) != O.word(i)) return false;
    return true;
  }

  bool operator!=(const APInt& O) const { return !(*this == O); }

  bool eq(const APInt& O) const { return *this == O; }

  bool ugt(const APInt& O) const {
    assert(BitWidth == O.BitWidth && "bitwidth mismatch");
    for (int i = static_cast<int>(NumWords) - 1; i >= 0; --i) {
      if (word(i) > O.word(i)) return true;
      if (word(i) < O.word(i)) return false;
    }
    return false;
  }

  bool uge(const APInt& O) const { return !O.ugt(*this); }
  bool ult(const APInt& O) const { return O.ugt(*this); }
  bool ule(const APInt& O) const { return !ugt(O); }

  bool sgt(const APInt& O) const {
    assert(BitWidth == O.BitWidth && "bitwidth mismatch");
    bool ANeg = isNegative();
    bool BNeg = O.isNegative();
    if (ANeg && !BNeg) return false;
    if (!ANeg && BNeg) return true;
    return ugt(O);
  }

  bool sge(const APInt& O) const { return !O.sgt(*this); }
  bool slt(const APInt& O) const { return O.sgt(*this); }
  bool sle(const APInt& O) const { return !sgt(O); }

  APInt operator+(const APInt& O) const {
    assert(BitWidth == O.BitWidth && "bitwidth mismatch");
    APInt Result(BitWidth, uint64_t(0));
    uint64_t Carry = 0;
    for (unsigned i = 0; i < NumWords; ++i) {
      uint64_t A = word(i);
      uint64_t B = O.word(i);
      uint64_t Sum = A + B + Carry;
      Carry = (Sum < A) || (Carry && Sum == A) ? 1 : 0;
      Result.setWord(i, Sum);
    }
    Result.clearUnusedBits();
    return Result;
  }

  APInt operator-(const APInt& O) const {
    assert(BitWidth == O.BitWidth && "bitwidth mismatch");
    APInt Result(BitWidth, uint64_t(0));
    uint64_t Borrow = 0;
    for (unsigned i = 0; i < NumWords; ++i) {
      uint64_t A = word(i);
      uint64_t B = O.word(i);
      uint64_t Diff = A - B - Borrow;
      Borrow = (A < B) || (Borrow && A == B) ? 1 : 0;
      Result.setWord(i, Diff);
    }
    Result.clearUnusedBits();
    return Result;
  }

  APInt operator-(void) const { return APInt(BitWidth, uint64_t(0)) - *this; }

  APInt operator*(const APInt& O) const {
    assert(BitWidth == O.BitWidth && "bitwidth mismatch");
    APInt Result(BitWidth, uint64_t(0));
    for (unsigned i = 0; i < NumWords; ++i) {
      uint64_t Carry = 0;
      for (unsigned j = 0; j < NumWords && (i + j) < NumWords; ++j) {
        unsigned K = i + j;
        __uint128_t Prod = static_cast<__uint128_t>(word(i)) * O.word(j);
        uint64_t Low = static_cast<uint64_t>(Prod) + Result.word(K) + Carry;
        Carry = static_cast<uint64_t>(Prod >> 64) + (Low < static_cast<uint64_t>(Prod) ? 1 : 0);
        Result.setWord(K, Low);
      }
    }
    Result.clearUnusedBits();
    return Result;
  }

  APInt udiv(const APInt& O) const;
  APInt sdiv(const APInt& O) const;
  APInt urem(const APInt& O) const;
  APInt srem(const APInt& O) const;

  APInt operator<<(unsigned Shift) const {
    if (BitWidth == 0 || Shift == 0) return *this;
    if (Shift >= BitWidth) return getZero(BitWidth);
    APInt Result(BitWidth, uint64_t(0));
    unsigned WordShift = Shift / 64;
    unsigned BitShift = Shift % 64;
    for (int i = static_cast<int>(NumWords) - 1; i >= 0; --i) {
      int Dst = i + static_cast<int>(WordShift);
      if (Dst >= static_cast<int>(NumWords)) continue;
      uint64_t W = word(i);
      if (BitShift == 0) {
        Result.setWord(Dst, W);
      } else {
        Result.setWord(Dst, Result.word(Dst) | (W << BitShift));
        if (Dst + 1 < static_cast<int>(NumWords)) {
          Result.setWord(Dst + 1, Result.word(Dst + 1) | (W >> (64 - BitShift)));
        }
      }
    }
    Result.clearUnusedBits();
    return Result;
  }

  APInt operator<<(const APInt& ShiftAmt) const {
    return *this << static_cast<unsigned>(ShiftAmt.getZExtValue());
  }

  APInt operator>>(unsigned Shift) const {
    if (BitWidth == 0 || Shift == 0) return *this;
    if (Shift >= BitWidth) return getZero(BitWidth);
    APInt Result(BitWidth, uint64_t(0));
    unsigned WordShift = Shift / 64;
    unsigned BitShift = Shift % 64;
    for (int i = static_cast<int>(NumWords) - 1; i >= 0; --i) {
      int Src = i + static_cast<int>(WordShift);
      if (Src >= static_cast<int>(NumWords)) continue;
      uint64_t W = word(Src);
      if (BitShift == 0) {
        Result.setWord(i, W);
      } else {
        Result.setWord(i, W >> BitShift);
        if (Src + 1 < static_cast<int>(NumWords)) {
          Result.setWord(i, Result.word(i) | (word(Src + 1) << (64 - BitShift)));
        }
      }
    }
    return Result;
  }

  APInt operator>>(const APInt& ShiftAmt) const {
    return *this >> static_cast<unsigned>(ShiftAmt.getZExtValue());
  }

  APInt ashr(unsigned Shift) const {
    if (BitWidth == 0 || Shift == 0) return *this;
    if (Shift >= BitWidth) return isNegative() ? getAllOnes(BitWidth) : getZero(BitWidth);
    APInt Result = *this >> Shift;
    if (isNegative()) {
      for (unsigned i = BitWidth - Shift; i < BitWidth; ++i)
        Result.setBit(i);
    }
    return Result;
  }

  APInt lshr(unsigned Shift) const { return *this >> Shift; }

  APInt operator&(const APInt& O) const {
    assert(BitWidth == O.BitWidth && "bitwidth mismatch");
    APInt Result(BitWidth, uint64_t(0));
    for (unsigned i = 0; i < NumWords; ++i)
      Result.setWord(i, word(i) & O.word(i));
    return Result;
  }

  APInt operator|(const APInt& O) const {
    assert(BitWidth == O.BitWidth && "bitwidth mismatch");
    APInt Result(BitWidth, uint64_t(0));
    for (unsigned i = 0; i < NumWords; ++i)
      Result.setWord(i, word(i) | O.word(i));
    return Result;
  }

  APInt operator^(const APInt& O) const {
    assert(BitWidth == O.BitWidth && "bitwidth mismatch");
    APInt Result(BitWidth, uint64_t(0));
    for (unsigned i = 0; i < NumWords; ++i)
      Result.setWord(i, word(i) ^ O.word(i));
    return Result;
  }

  APInt operator~() const {
    APInt Result(BitWidth, uint64_t(0));
    for (unsigned i = 0; i < NumWords; ++i)
      Result.setWord(i, ~word(i));
    Result.clearUnusedBits();
    return Result;
  }

  APInt& operator&=(const APInt& O) {
    assert(BitWidth == O.BitWidth && "bitwidth mismatch");
    for (unsigned i = 0; i < NumWords; ++i)
      setWord(i, word(i) & O.word(i));
    return *this;
  }

  APInt& operator|=(const APInt& O) {
    assert(BitWidth == O.BitWidth && "bitwidth mismatch");
    for (unsigned i = 0; i < NumWords; ++i)
      setWord(i, word(i) | O.word(i));
    return *this;
  }

  APInt& operator^=(const APInt& O) {
    assert(BitWidth == O.BitWidth && "bitwidth mismatch");
    for (unsigned i = 0; i < NumWords; ++i)
      setWord(i, word(i) ^ O.word(i));
    return *this;
  }

  APInt& operator<<=(unsigned Shift) {
    *this = *this << Shift;
    return *this;
  }

  APInt& operator>>=(unsigned Shift) {
    *this = *this >> Shift;
    return *this;
  }

  APInt rotl(unsigned Rotate) const {
    if (BitWidth == 0) return *this;
    Rotate %= BitWidth;
    if (Rotate == 0) return *this;
    return (*this << Rotate) | (*this >> (BitWidth - Rotate));
  }

  APInt rotr(unsigned Rotate) const {
    if (BitWidth == 0) return *this;
    Rotate %= BitWidth;
    if (Rotate == 0) return *this;
    return (*this >> Rotate) | (*this << (BitWidth - Rotate));
  }

  unsigned countLeadingZeros() const {
    if (BitWidth == 0) return 0;
    for (int i = static_cast<int>(NumWords) - 1; i >= 0; --i) {
      uint64_t W = word(i);
      if (W != 0) {
        unsigned LZ = __builtin_clzll(W);
        unsigned BitsInWord = (i == static_cast<int>(NumWords) - 1)
          ? (BitWidth - i * 64) : 64;
        unsigned WordLZ = 64 - BitsInWord;
        return static_cast<unsigned>(i) * 64 + WordLZ + (LZ - WordLZ > LZ ? 0 : LZ - WordLZ);
      }
    }
    return BitWidth;
  }

  unsigned countLeadingOnes() const {
    return (~*this).countLeadingZeros();
  }

  unsigned countTrailingZeros() const {
    if (BitWidth == 0) return 0;
    for (unsigned i = 0; i < NumWords; ++i) {
      uint64_t W = word(i);
      if (W != 0) return i * 64 + static_cast<unsigned>(__builtin_ctzll(W));
    }
    return BitWidth;
  }

  unsigned countTrailingOnes() const {
    return (~*this).countTrailingZeros();
  }

  unsigned countPopulation() const {
    unsigned Count = 0;
    for (unsigned i = 0; i < NumWords; ++i)
      Count += static_cast<unsigned>(__builtin_popcountll(word(i)));
    return Count;
  }

  std::string toString(unsigned Radix = 10, bool IsSigned = false) const;

  void print(raw_ostream& OS) const;

  static APInt fromString(StringRef Str, unsigned BitWidth, bool IsSigned = false);
};

} // namespace ir
} // namespace blocktype

#endif // BLOCKTYPE_IR_ADT_APINT_H
