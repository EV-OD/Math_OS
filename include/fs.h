#ifndef FS_H
#define FS_H

void fs_init(void);
int fs_mkdir(const char *path);
int fs_write(const char *path, const char *data, int len);
int fs_read(const char *path, char *out, int cap);
int fs_list(const char *path, char *out, int cap);
int fs_isdir(const char *path);
int fs_exists(const char *path);
const char *fs_cwd(void);
int fs_cd(const char *path);

#endif
