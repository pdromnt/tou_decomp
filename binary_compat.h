#ifndef TOU_BINARY_COMPAT_H
#define TOU_BINARY_COMPAT_H

#include <stddef.h>
#include <stdint.h>

namespace tou_binary {

uint8_t load_u8(const void *base, size_t offset);
uint16_t load_u16(const void *base, size_t offset);
uint32_t load_u32(const void *base, size_t offset);
int32_t load_i32(const void *base, size_t offset);
void store_u8(void *base, size_t offset, uint8_t value);
void store_u16(void *base, size_t offset, uint16_t value);
void store_u32(void *base, size_t offset, uint32_t value);
void store_i32(void *base, size_t offset, int32_t value);

int32_t add_wrap_i32(int32_t left, int32_t right);
int32_t sub_wrap_i32(int32_t left, int32_t right);
int32_t mul_wrap_i32(int32_t left, int32_t right);
int32_t shl_wrap_i32(int32_t value, unsigned int shift);
int32_t sar_i32(int32_t value, unsigned int shift);

class MsvcRng {
public:
    explicit MsvcRng(uint32_t seed = 1u);
    void seed(uint32_t value);
    int next();
    uint32_t state() const;

private:
    uint32_t state_;
};

int64_t x87_ftol(long double value);
uint16_t x87_control_word();

} // namespace tou_binary

extern "C" int TOU_Rand(void);
extern "C" void TOU_Srand(unsigned int seed);
extern "C" uint32_t TOU_RandState(void);
extern "C" uint64_t TOU_RandCallCount(void);

#endif
