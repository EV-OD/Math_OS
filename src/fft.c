#include <fft.h>
#include <kmath.h>

int fft_valid_n(int n) {
    return n >= 16 && n <= 256 && (n & (n - 1)) == 0;
}

float fft_mag(Complex c) {
    return k_sqrt(c.re * c.re + c.im * c.im);
}

void fft_inplace(Complex *x, int n) {
    int j = 0;
    for (int i = 1; i < n; i++) {
        int bit = n >> 1;
        while (j & bit) { j ^= bit; bit >>= 1; }
        j ^= bit;
        if (i < j) {
            Complex t = x[i];
            x[i] = x[j];
            x[j] = t;
        }
    }
    for (int len = 2; len <= n; len <<= 1) {
        float ang = -2.0f * K_PI / (float)len;
        float wr = k_cos(ang);
        float wi = k_sin(ang);
        for (int i = 0; i < n; i += len) {
            float cur_r = 1.0f, cur_i = 0.0f;
            for (int k = 0; k < len / 2; k++) {
                Complex u = x[i + k];
                Complex v = x[i + k + len / 2];
                float tr = v.re * cur_r - v.im * cur_i;
                float ti = v.re * cur_i + v.im * cur_r;
                x[i + k].re = u.re + tr;
                x[i + k].im = u.im + ti;
                x[i + k + len / 2].re = u.re - tr;
                x[i + k + len / 2].im = u.im - ti;
                float nr = cur_r * wr - cur_i * wi;
                cur_i = cur_r * wi + cur_i * wr;
                cur_r = nr;
            }
        }
    }
}
