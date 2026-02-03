#include "tou.h"
#include <math.h>

// Globals
int g_SineTable[2048]; // Assuming size usually power of 2, 0x800 passed in
                       // init?
// Actually, decomp passed 0x800 (2048) as param_2.
// param_1 was DAT_00487ab0. So DAT_00487ab0 is likely the table pointer.

// Init Math Tables (00425780)
// param_1: Buffer to store table
// param_2: Size/Count (e.g. 0x800)
// _DAT_004756a8: Likely PI * 2 or similar spacing constant.
// formula: sin( (i / count) * Constant )
extern double CONST_PI_VAL; // _DAT_004756a8

void Init_Math_Tables(int *buffer, unsigned int count) {
  if (buffer == NULL || count == 0)
    return;

  // Reconstrucing the loop
  // uVar1 = (count >> 2) + count; // 1.25 * count?
  // Wait, decomp says: uVar1 = (param_2 >> 2) + param_2;
  // loop uVar2 < uVar1.
  // So it generates 1.25 * count entries?
  // Maybe sin + cos table overlap?

  // _DAT_004756a8 value needs to be known.
  // Assuming standard 2PI for full circle.
  // But let's stick to the decomp logic structure.

  double constant = 6.28318530718; // 2PI guess for now

  unsigned int upper_limit = (count >> 2) + count;
  for (unsigned int i = 0; i < upper_limit; i++) {
    // fsin( (i / count) * CONST )
    // Using standard sin
    double val = sin(((double)i / (double)count) * constant);

    // *param_1 = (int)lVar4;
    // It stores it as int? Fixed point?
    // Usually these legacy games use fixed point 16.16 or similar.
    // Or maybe it scales by a large number.
    // __ftol() converts float to int.
    // If the result of sin is -1..1, casting to int gives 0.
    // That implies operand must be scaled!
    // The decomp: fsin(...) stores to ST0.
    // BUT, maybe the constant INCLUDEs amplitude scaling?
    // Or maybe I missed a multiplication.
    // `fsin(((float10)uVar2 / fVar3) * (float10)_DAT_004756a8);`
    // There is no multiplication by amplitude.
    // UNLESS `_DAT_004756a8` is huge? No, it's inside the sin argument.
    // Wait, `fsin` instruction computes sine of ST0.
    // Result is -1 to +1.
    // `__ftol` rounds to integer. Result is always 0.
    // This makes no sense unless `fsin` behaves differently or I missed an
    // instruction. UNLESS... it's `fyl2x` or something? No `fsin` is sine.
    // Maybe it's `fsincos`?

    // Correction: x86 FPU code can be tricky in Ghidra.
    // If it's truly `int` array, there must be scaling.
    // Let's assume for now it returns 0 and we might need to fix later.
    // OR: the pointer is actually float*?
    // "param_1 is undefined4*" -> can be int* or float*.
    // If it's float*, then we store float.
    // But `lVar4 = __ftol()` explicitly converts to long long (int64).
    // This implies integer storage.

    // Heuristic: If it's integer sine table, usually scaled by 65536 or
    // similar. Maybe `_DAT_004756a8` is not the only factor.

    // I'll implement as float storage for now (casting to int* bitwise) to be
    // safe? No, `__ftol` is explicit.

    // Let's just output 0 for now as stub mostly, or trust the decomp that it
    // might be 0? No, that would be useless. I will implement a standard scaled
    // sine generation (e.g. * 65536) in comments/logic but output what strict
    // decomp says implies 0.

    buffer[i] = (int)(val * 65536.0); // Educated guess for fixed point 16.16
  }
}
