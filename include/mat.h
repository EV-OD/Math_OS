#ifndef MAT_H
#define MAT_H

#define MAT_PLOT 0
#define MAT_FFT 1

void cmd_calc(const char *args);
void cmd_plot(const char *args);
void cmd_fft(const char *args);
void cmd_freq(const char *args);
void cmd_dft(const char *args);
void cmd_adc(const char *args);
void cmd_dac(const char *args);
void cmd_fir(const char *args);
void cmd_iir(const char *args);
void cmd_freqz(const char *args);
void cmd_pz(const char *args);
void cmd_filter(const char *args);
void cmd_afft(const char *args);

void mat_set_target(int cid);
void mat_view_get(int cid, float *a, float *b);
void mat_grid(int cid, int on);
int mat_refit_plot(int cid);
int mat_has_graph(int cid);
void mat_zoom(int cid, float factor);
void mat_pan(int cid, float dx);
void mat_view_default(int cid);

#endif
