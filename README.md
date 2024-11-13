# LLVM Abstract Transfer Function

## Overview

This project analyzes the transfer functions in LLVM's **KnownBits** abstract domain for the **rotate left** operation. The aim is to compare the precision and soundness of the composite transfer function against a decomposed version that uses simpler LLVM instructions (`shl`, `lshr`, and `or`).

## Transfer Functions Implemented

- **Composite Transfer Function for Rotate Left (`knownBitsRotateLeft`)**: Directly computes the known bits after a rotate left operation in the KnownBits domain.
- **Decomposed Transfer Function (`knownBitsRotateLeftDecomposed`)**: Simulates the rotate left operation using shift left (`shl`), logical shift right (`lshr`), and bitwise OR (`or`) operations.

## Building the Project

### Prerequisites

- **LLVM**: Ensure LLVM is installed on your system. Download it from [https://llvm.org/](https://llvm.org/).
- **C++ Compiler**: A compiler supporting C++14 or later (e.g., `clang++`, `g++`).
- **`llvm-config`**: Should be accessible from the command line to provide compiler and linker flags.

### Compilation Instructions

Use the following command to compile the code:

```bash
clang++ -std=c++14 -O3 main.cpp -o knownbits_rotateleft.o `llvm-config --cxxflags --ldflags --libs core support` -lunwind
```

## Running the Program

After compiling, execute the program with:

```bash
./knownbits_rotateleft.o
```

The program will test the transfer functions for bitwidths from 4 to 6 and output the comparison results.

## Sample Output

```
Testing with bitwidth: 4
Total number of abstract values: 81
Composite transfer function more precise: 0
Decomposed transfer function more precise: 0
Equal precision: 81
Incomparable results: 0

Testing with bitwidth: 5
Total number of abstract values: 243
Composite transfer function more precise: 0
Decomposed transfer function more precise: 0
Equal precision: 243
Incomparable results: 0

Testing with bitwidth: 6
Total number of abstract values: 729
Composite transfer function more precise: 0
Decomposed transfer function more precise: 0
Equal precision: 729
Incomparable results: 0
```

## Explanation of Results

- **Total number of abstract values**: The total number of possible KnownBits abstract values for the given bitwidth.
- **Composite transfer function more precise**: Cases where the composite transfer function resulted in more precise known bits.
- **Decomposed transfer function more precise**: Cases where the decomposed transfer function was more precise (expected to be zero).
- **Equal precision**: Cases where both transfer functions yielded the same precision.
- **Incomparable results**: Cases where the results are not directly comparable (expected to be zero).