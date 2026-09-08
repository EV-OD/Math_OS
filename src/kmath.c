#include <kmath.h>
#include <stdint.h>

#define LN2 0.69314718f
#define LOG2E 1.44269504f

float k_fabs(float x) {
    union { float f; uint32_t u; } v;
    v.f = x;
    v.u &= 0x7FFFFFFFu;
    return v.f;
}

int k_isfinite(float x) {
    union { float f; uint32_t u; } v;
    v.f = x;
    return ((v.u >> 23) & 0xFF) != 0xFF;
}

float k_sin(float x) {
    float s, c;
    __asm__ volatile("fsincos" : "=t"(c), "=u"(s) : "0"(x));
    (void)c;
    return s;
}

float k_cos(float x) {
    float s, c;
    __asm__ volatile("fsincos" : "=t"(c), "=u"(s) : "0"(x));
    (void)s;
    return c;
}

float k_tan(float x) {
    return k_sin(x) / k_cos(x);
}

float k_sqrt(float x) {
    float r;
    __asm__ volatile("fsqrt" : "=t"(r) : "0"(x));
    return r;
}

float k_exp(float x) {
    if (x != x) return x;
    if (x > 88.0f) return 1e38f;
    if (x < -104.0f) return 0.0f;
    float y = x * LOG2E;
    long k = (long)(y + (y >= 0.0f ? 0.5f : -0.5f));
    float r = x - (float)k * LN2;
    float r2 = r * r;
    float p = 1.0f + r + r2 * 0.5f + r2 * r * 0.16666667f +
              r2 * r2 * 0.041666667f + r2 * r2 * r * 0.008333333f +
              r2 * r2 * r2 * 0.0013888889f + r2 * r2 * r2 * r * 0.0001984127f;
    union { float f; uint32_t u; } v;
    v.f = p;
    int e = (int)((v.u >> 23) & 0xFF) + (int)k;
    if (e >= 255) return 1e38f;
    if (e <= 0) return 0.0f;
    v.u = (v.u & 0x807FFFFFu) | ((uint32_t)e << 23);
    return v.f;
}

float k_log(float x) {
    if (x != x) return x;
    if (x <= 0.0f) return -1e38f;
    union { float f; uint32_t u; } v;
    v.f = x;
    int e = (int)((v.u >> 23) & 0xFF) - 127;
    v.u = (v.u & 0x807FFFFFu) | (127u << 23);
    float m = v.f;
    float r = (m - 1.0f) / (m + 1.0f);
    float r2 = r * r;
    float t = r;
    float p = r;
    t *= r2; p += t * 0.33333333f;
    t *= r2; p += t * 0.2f;
    t *= r2; p += t * 0.14285714f;
    t *= r2; p += t * 0.11111111f;
    t *= r2; p += t * 0.090909091f;
    return 2.0f * p + (float)e * LN2;
}

float k_log10(float x) {
    return k_log(x) * 0.43429448f;
}

float k_pow(float x, float y) {
    if (y == 0.0f) return 1.0f;
    if (x == 0.0f) {
        if (y > 0.0f) return 0.0f;
        float z = x - x;
        return z / z;
    }
    if (x < 0.0f) {
        float z = x - x;
        return z / z;
    }
    return k_exp(y * k_log(x));
}

float k_floor(float x) {
    if (x != x) return x;
    if (x >= 2147483647.0f || x <= -2147483648.0f) return x;
    long t = (long)x;
    float f = (float)t;
    if (f > x) t--;
    return (float)t;
}

float k_ceil(float x) {
    return -k_floor(-x);
}

float k_atan2(float y, float x) {
    float r;
    __asm__ volatile("fpatan" : "=t"(r) : "0"(x), "u"(y));
    return r;
}

float k_fmod(float a, float b) {
    if (b == 0.0f) {
        float z = a - a;
        return z / z;
    }
    float q = a / b;
    if (q > 2000000000.0f || q < -2000000000.0f) return a;
    long t = (long)q;
    return a - (float)t * b;
}
