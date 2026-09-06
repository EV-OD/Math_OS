#ifndef FFT_H
#define FFT_H

typedef struct { float re, im; } Complex;

void fft_inplace(Complex *x, int n);
float fft_mag(Complex c);
int fft_valid_n(int n);

#endif
