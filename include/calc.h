#ifndef CALC_H
#define CALC_H

#define CALC_OK 0
#define CALC_SYNTAX 1
#define CALC_DIV0 2
#define CALC_UNKNOWN 3
#define CALC_DOMAIN 4
#define CALC_DEPTH 5

#define CALC_NPARAMS 4
#define CALC_BODYLEN 220

float calc_eval(const char *s, int *err);
void calc_set_var(const char *name, float val);
float calc_get_var(const char *name, int *found);
const char *calc_errstr(int err);
int calc_last_was_def(void);
const char *calc_last_sig(void);

#endif
