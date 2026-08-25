/*
 * math.cpp - Math utilities, trigonometry
 *
 * Original address: Init_Math_Tables=00425780
 */
#include "tou.h"

#include <math.h>

/* Generates the original 2^19 fixed-point sine table with an extra quarter
 * turn appended so callers can index cosine without a second table. Keep the
 * double evaluation and truncating cast: gameplay depends on these exact
 * integer entries. */
void Init_Math_Tables(int *buffer, unsigned int count)
{
    const unsigned int total = count + (count >> 2);
    const double two_pi = 6.283185307179586;
    for (unsigned int i = 0; i < total; ++i) {
        const double angle = (static_cast<double>(i) / count) * two_pi;
        buffer[i] = static_cast<int>(sin(angle) * 524288.0);
    }
}
