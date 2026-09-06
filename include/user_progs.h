#ifndef USER_PROGS_H
#define USER_PROGS_H

typedef struct {
    const char *name;
    void (*entry)(void);
    int user;
    const char *desc;
} user_prog_t;

extern const user_prog_t user_progs[];
extern const int user_progs_count;

const user_prog_t *user_find(const char *name);

void prog_counter_a(void);
void prog_counter_b(void);
void prog_spinner(void);
void prog_uhello(void);
void prog_gfx(void);
void prog_glcube(void);

#endif
