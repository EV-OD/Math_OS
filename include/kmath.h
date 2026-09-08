#ifndef KMATH_H
#define KMATH_H

#define K_PI 3.14159265f
#define K_E 2.71828183f

float k_sin(float x);
float k_cos(float x);
float k_tan(float x);
float k_sqrt(float x);
float k_exp(float x);
float k_log(float x);
float k_log10(float x);
float k_pow(float x, float y);
float k_fabs(float x);
float k_floor(float x);
float k_ceil(float x);
float k_fmod(float a, float b);
float k_atan2(float y, float x);
int k_isfinite(float x);

#endif
