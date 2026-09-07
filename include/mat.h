#ifndef MAT_H
#define MAT_H

#define MAT_PLOT 0
#define MAT_FFT 1

void cmd_calc(const char *args);
void cmd_plot(const char *args);
void cmd_fft(const char *args);
void mat_remember(int cid, int kind, const char *args);
int mat_recall(int cid, int *kind, char *out);

#endif
