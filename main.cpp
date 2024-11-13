#include <cassert>
#include <iostream>
#include <llvm/ADT/APInt.h>
#include <llvm/Support/KnownBits.h>
#include <set>
#include <vector>

// Comparator for llvm::APInt
struct APIntCompare {
  bool operator()(const llvm::APInt &lhs, const llvm::APInt &rhs) const {
    return lhs.ult(rhs); // Use unsigned less-than comparison
  }
};

// Function to generate all possible KnownBits for a given bitwidth
void generateAllKnownBits(unsigned BitWidth,
                          std::vector<llvm::KnownBits> &AllKnownBits) {
  size_t NumValues = 1;
  for (unsigned i = 0; i < BitWidth; ++i)
    NumValues *= 3; // 3 choices per bit

  for (size_t i = 0; i < NumValues; ++i) {
    llvm::KnownBits KB(BitWidth);
    size_t idx = i;
    bool Overlap = false;
    for (unsigned bit = 0; bit < BitWidth; ++bit) {
      unsigned rem = idx % 3;
      idx /= 3;
      if (rem == 0) {
        // Bit is unknown
        KB.Zero.clearBit(bit);
        KB.One.clearBit(bit);
      } else if (rem == 1) {
        // Bit is known zero
        KB.Zero.setBit(bit);
        KB.One.clearBit(bit);
      } else { // rem == 2
        // Bit is known one
        KB.Zero.clearBit(bit);
        KB.One.setBit(bit);
      }
      if (KB.Zero[bit] && KB.One[bit]) {
        Overlap = true;
        break;
      }
    }
    if (!Overlap)
      AllKnownBits.push_back(KB);
  }
}

// Concretization function for KnownBits
void concretizeKnownBits(const llvm::KnownBits &KB,
                         std::set<llvm::APInt, APIntCompare> &ConcreteValues) {
  unsigned BitWidth = KB.getBitWidth();
  llvm::APInt KnownZero = KB.Zero;
  llvm::APInt KnownOne = KB.One;
  llvm::APInt Value(BitWidth, 0);
  unsigned NumUnknownBits = 0;
  std::vector<unsigned> UnknownBitPositions;
  for (unsigned i = 0; i < BitWidth; ++i) {
    if (!KnownZero[i] && !KnownOne[i]) {
      UnknownBitPositions.push_back(i);
      ++NumUnknownBits;
    } else if (KnownOne[i]) {
      Value.setBit(i);
    }
    // otherwise it is zero
  }
  size_t NumValues = 1ULL << NumUnknownBits;
  for (size_t i = 0; i < NumValues; ++i) {
    llvm::APInt ConcreteValue = Value;
    for (unsigned j = 0; j < UnknownBitPositions.size(); ++j) {
      unsigned BitPos = UnknownBitPositions[j];
      if (i & (1ULL << j))
        ConcreteValue.setBit(BitPos);
      else
        ConcreteValue.clearBit(BitPos);
    }
    ConcreteValues.insert(ConcreteValue);
  }
}

// Abstraction function for KnownBits
llvm::KnownBits
abstractValues(const std::set<llvm::APInt, APIntCompare> &ConcreteValues) {
  assert(!ConcreteValues.empty() && "ConcreteValues cannot be empty");
  unsigned BitWidth = ConcreteValues.begin()->getBitWidth();
  llvm::APInt KnownZero(BitWidth, 0);
  llvm::APInt KnownOne(BitWidth, ~0ULL);

  // Find all bits with zero or one
  for (const llvm::APInt &Val : ConcreteValues) {
    KnownZero |= ~Val;
    KnownOne &= Val;
  }
  llvm::KnownBits KB(BitWidth);
  KB.Zero = KnownZero;
  KB.One = KnownOne;
  return KB;
}

// Composite transfer function for rotate left
llvm::KnownBits knownBitsRotateLeft(const llvm::KnownBits &X,
                                    unsigned ShiftAmount) {
  unsigned BitWidth = X.getBitWidth();
  ShiftAmount %= BitWidth; // skip the uncessary shift

  llvm::KnownBits Result(BitWidth);
  Result.Zero = (X.Zero.shl(ShiftAmount) | X.Zero.lshr(BitWidth - ShiftAmount));
  Result.One = (X.One.shl(ShiftAmount) | X.One.lshr(BitWidth - ShiftAmount));
  return Result;
}

llvm::KnownBits knownBitsShl(const llvm::KnownBits &X, unsigned ShiftAmount) {
  unsigned BitWidth = X.getBitWidth();
  llvm::KnownBits Result(BitWidth);
  if (ShiftAmount >= BitWidth) {
    Result.resetAll();
    return Result;
  }
  Result.Zero = X.Zero.shl(ShiftAmount);
  Result.One = X.One.shl(ShiftAmount);

  // Low bits become zero after shift
  Result.Zero.setLowBits(ShiftAmount);
  return Result;
}

llvm::KnownBits knownBitsLshr(const llvm::KnownBits &X, unsigned ShiftAmount) {
  unsigned BitWidth = X.getBitWidth();
  llvm::KnownBits Result(BitWidth);
  if (ShiftAmount >= BitWidth) {
    Result.resetAll();
    return Result;
  }
  Result.Zero = X.Zero.lshr(ShiftAmount);
  Result.One = X.One.lshr(ShiftAmount);
  Result.Zero.setHighBits(ShiftAmount); // High bits become zero after shift
  return Result;
}

llvm::KnownBits knownBitsOr(const llvm::KnownBits &A,
                            const llvm::KnownBits &B) {
  unsigned BitWidth = A.getBitWidth();
  llvm::KnownBits Result(BitWidth);
  Result.One = A.One | B.One;
  Result.Zero = A.Zero & B.Zero;
  return Result;
}

// Decomposed transfer function for rotate left
llvm::KnownBits knownBitsRotateLeftDecomposed(const llvm::KnownBits &X,
                                              unsigned ShiftAmount) {
  unsigned BitWidth = X.getBitWidth();
  ShiftAmount %= BitWidth;

  llvm::KnownBits LeftShifted = knownBitsShl(X, ShiftAmount);
  llvm::KnownBits RightShifted = knownBitsLshr(X, BitWidth - ShiftAmount);
  llvm::KnownBits Result = knownBitsOr(LeftShifted, RightShifted);
  return Result;
}

enum ComparisonResult {
  Equal,
  CompositeMorePrecise,
  DecomposedMorePrecise,
  Incomparable
};

// Function to compare the precision of two KnownBits
ComparisonResult compareKnownBits(const llvm::KnownBits &CompositeKB,
                                  const llvm::KnownBits &DecomposedKB) {
  bool CompositeAtLeastAsPrecise =
      ((CompositeKB.One | DecomposedKB.One) == CompositeKB.One) &&
      ((CompositeKB.Zero | DecomposedKB.Zero) == CompositeKB.Zero);

  bool DecomposedAtLeastAsPrecise =
      ((DecomposedKB.One | CompositeKB.One) == DecomposedKB.One) &&
      ((DecomposedKB.Zero | CompositeKB.Zero) == DecomposedKB.Zero);

  if (CompositeKB.One == DecomposedKB.One &&
      CompositeKB.Zero == DecomposedKB.Zero)
    return Equal;
  else if (CompositeAtLeastAsPrecise && !DecomposedAtLeastAsPrecise)
    return CompositeMorePrecise;
  else if (DecomposedAtLeastAsPrecise && !CompositeAtLeastAsPrecise)
    return DecomposedMorePrecise;
  else
    return Incomparable;
}

// Function to test the transfer functions and collect statistics
void testTransferFunctions(unsigned BitWidth, unsigned ShiftAmount) {
  std::vector<llvm::KnownBits> AllKnownBits;
  generateAllKnownBits(BitWidth, AllKnownBits);

  size_t TotalValues = AllKnownBits.size();
  size_t CompositeMorePreciseCount = 0;
  size_t DecomposedMorePreciseCount = 0;
  size_t IncomparableCount = 0;
  size_t EqualCount = 0;

  for (const llvm::KnownBits &X : AllKnownBits) {
    llvm::KnownBits CompositeResult = knownBitsRotateLeft(X, ShiftAmount);
    llvm::KnownBits DecomposedResult =
        knownBitsRotateLeftDecomposed(X, ShiftAmount);

    ComparisonResult CompResult =
        compareKnownBits(CompositeResult, DecomposedResult);

    if (CompResult == CompositeMorePrecise)
      ++CompositeMorePreciseCount;
    else if (CompResult == DecomposedMorePrecise)
      ++DecomposedMorePreciseCount;
    else if (CompResult == Incomparable)
      ++IncomparableCount;
    else if (CompResult == Equal)
      ++EqualCount;
  }

  std::cout << "Total number of abstract values: " << TotalValues << "\n";
  std::cout << "Composite transfer function more precise: "
            << CompositeMorePreciseCount << "\n";
  std::cout << "Decomposed transfer function more precise: "
            << DecomposedMorePreciseCount << "\n";
  std::cout << "Equal precision: " << EqualCount << "\n";
  std::cout << "Incomparable results: " << IncomparableCount << "\n";
}

int main() {
  for (unsigned BitWidth = 4; BitWidth <= 6; ++BitWidth) {
    std::cout << "Testing with bitwidth: " << BitWidth << "\n";
    testTransferFunctions(BitWidth, 1);
    std::cout << "\n";
  }
  return 0;
}
