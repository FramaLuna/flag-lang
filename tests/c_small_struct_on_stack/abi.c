#include <stdint.h>
typedef struct { uint8_t a; } Byte;
typedef struct { int16_t a; } Half;
typedef struct { float a; } Single;
int64_t small_on_stack(int64_t a0, int64_t a1, int64_t a2, int64_t a3, int64_t a4, int64_t a5, int64_t a6, int64_t a7, Byte b, int64_t after_b, Half h, int64_t after_h, Single s, int64_t after_s) {
    return b.a * 100000 + after_b * 10000 + h.a * 1000 + after_h * 100 + (int64_t)s.a * 10 + after_s;
}
int64_t flag_small_on_stack(int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, Byte, int64_t, Half, int64_t, Single, int64_t);
int64_t call_back(void) {
    Byte b = {7};
    Half h = {9};
    Single s = {2};
    return flag_small_on_stack(0, 0, 0, 0, 0, 0, 0, 0, b, 8, h, 1, s, 3);
}
