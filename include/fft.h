#ifndef FFT_H
#define FFT_H

typedef struct { float re, im; } Complex;

void fft_inplace(Complex *x, int n);
void dft_direct(Complex *x, int n);
float fft_mag(Complex c);
int fft_valid_n(int n);
int dft_valid_n(int n);

#endif

