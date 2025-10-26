#include "types.h"

union FloatBits
{
    f32 f;
    u32 ui;
};

// Impl taken from meshoptimizer
// TODO: Newer CPU with F16C should have special instruction for conversion!
u16 f16::QuantizeHalf(f32 v)
{
    FloatBits u = {v};
    u32 ui = u.ui;

    i32 s = (ui >> 16) & 0x8000;
    i32 em = ui & 0x7fffffff;

    // bias exponent and round to nearest; 112 is relative exponent bias
    // (127-15)
    i32 h = (em - (112 << 23) + (1 << 12)) >> 13;

    // underflow: flush to zero; 113 encodes exponent -14
    h = (em < (113 << 23)) ? 0 : h;

    // overflow: infinity; 143 encodes exponent 16
    h = (em >= (143 << 23)) ? 0x7c00 : h;

    // NaN; note that we convert all types of NaN to qNaN
    h = (em > (255 << 23)) ? 0x7e00 : h;

    return (u16)(s | h);
}

f32 f16::DequantizeHalf(f16 h)
{
    u32 s = u32(h.data & 0x8000) << 16;
    i32 em = h.data & 0x7fff;

    // bias exponent and pad mantissa with 0; 112 is relative exponent bias
    // (127-15)
    i32 r = (em + (112 << 10)) << 13;

    // denormal: flush to zero
    r = (em < (1 << 10)) ? 0 : r;

    // infinity/NaN; note that we preserve NaN payload as a byproduct of
    // unifying inf/nan cases 112 is an exponent bias fixup; since we already
    // applied it once, applying it twice converts 31 to 255
    r += (em >= (31 << 10)) ? (112 << 23) : 0;

    FloatBits u;
    u.ui = s | r;
    return u.f;
}
