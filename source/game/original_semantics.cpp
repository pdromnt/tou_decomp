#include "original_semantics.h"

#include <math.h>
#include <string.h>

namespace {

tou_binary::MsvcRng g_rng(1u);
uint64_t g_rng_call_count = 0;

int32_t bits_to_i32(uint32_t value)
{
    int32_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

} // namespace

namespace tou_binary {

uint8_t load_u8(const void *base, size_t offset)
{
    return static_cast<const uint8_t *>(base)[offset];
}

uint16_t load_u16(const void *base, size_t offset)
{
    const uint8_t *p = static_cast<const uint8_t *>(base) + offset;
    return static_cast<uint16_t>(p[0]) |
           static_cast<uint16_t>(static_cast<uint16_t>(p[1]) << 8);
}

uint32_t load_u32(const void *base, size_t offset)
{
    const uint8_t *p = static_cast<const uint8_t *>(base) + offset;
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

int32_t load_i32(const void *base, size_t offset)
{
    return bits_to_i32(load_u32(base, offset));
}

void store_u8(void *base, size_t offset, uint8_t value)
{
    static_cast<uint8_t *>(base)[offset] = value;
}

void store_u16(void *base, size_t offset, uint16_t value)
{
    uint8_t *p = static_cast<uint8_t *>(base) + offset;
    p[0] = static_cast<uint8_t>(value);
    p[1] = static_cast<uint8_t>(value >> 8);
}

void store_u32(void *base, size_t offset, uint32_t value)
{
    uint8_t *p = static_cast<uint8_t *>(base) + offset;
    p[0] = static_cast<uint8_t>(value);
    p[1] = static_cast<uint8_t>(value >> 8);
    p[2] = static_cast<uint8_t>(value >> 16);
    p[3] = static_cast<uint8_t>(value >> 24);
}

void store_i32(void *base, size_t offset, int32_t value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    store_u32(base, offset, bits);
}

int32_t add_wrap_i32(int32_t left, int32_t right)
{
    return bits_to_i32(static_cast<uint32_t>(left) + static_cast<uint32_t>(right));
}

int32_t sub_wrap_i32(int32_t left, int32_t right)
{
    return bits_to_i32(static_cast<uint32_t>(left) - static_cast<uint32_t>(right));
}

int32_t mul_wrap_i32(int32_t left, int32_t right)
{
    return bits_to_i32(static_cast<uint32_t>(left) * static_cast<uint32_t>(right));
}

int32_t shl_wrap_i32(int32_t value, unsigned int shift)
{
    shift &= 31u;
    return bits_to_i32(static_cast<uint32_t>(value) << shift);
}

int32_t sar_i32(int32_t value, unsigned int shift)
{
    shift &= 31u;
    if (shift == 0u) return value;
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    uint32_t result = bits >> shift;
    if ((bits & 0x80000000u) != 0u) {
        result |= 0xffffffffu << (32u - shift);
    }
    return bits_to_i32(result);
}

MsvcRng::MsvcRng(uint32_t seed_value) : state_(seed_value) {}
void MsvcRng::seed(uint32_t value) { state_ = value; }

int MsvcRng::next()
{
    state_ = state_ * 0x343fdu + 0x269ec3u;
    return static_cast<int>((state_ >> 16) & 0x7fffu);
}

uint32_t MsvcRng::state() const { return state_; }

uint16_t x87_control_word()
{
#if defined(__GNUC__) && defined(__i386__)
    uint16_t result;
    /* Waiting form matches the original FSTCW/WAIT sequence at 0x0046448e. */
    __asm__ __volatile__("fstcw %0" : "=m"(result));
    return result;
#else
    return 0;
#endif
}

int64_t x87_ftol(long double value)
{
#if defined(__GNUC__) && defined(__i386__)
    uint16_t original = x87_control_word();
    uint16_t truncate = static_cast<uint16_t>(original | 0x0c00u);
    int64_t result;
    __asm__ __volatile__(
        "fldcw %1\n\t"
        "fldt %2\n\t"
        "fistpll %0\n\t"
        "fldcw %3"
        : "=m"(result)
        : "m"(truncate), "m"(value), "m"(original)
        : "st");
    return result;
#else
    return static_cast<int64_t>(truncl(value));
#endif
}

} // namespace tou_binary

extern "C" int TOU_Rand(void)
{
    ++g_rng_call_count;
    return g_rng.next();
}

extern "C" void TOU_Srand(unsigned int seed)
{
    g_rng.seed(static_cast<uint32_t>(seed));
    g_rng_call_count = 0;
}

extern "C" uint32_t TOU_RandState(void) { return g_rng.state(); }
extern "C" uint64_t TOU_RandCallCount(void) { return g_rng_call_count; }
extern "C" void TOU_RestoreRandState(uint32_t state, uint64_t call_count)
{
    g_rng.seed(state);
    g_rng_call_count = call_count;
}
