#ifndef TOU_FIXED_POINT_H
#define TOU_FIXED_POINT_H

/* World positions use 18 fractional bits: one whole unit is 1 << 18. */
#define FIXED_SHIFT 18
#define FIXED_SCALE 0x40000

#if FIXED_SCALE != (1 << FIXED_SHIFT)
#error "Fixed-point scale and shift disagree"
#endif

#endif /* TOU_FIXED_POINT_H */
