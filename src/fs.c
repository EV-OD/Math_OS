#include <fs.h>
#include <string.h>
#include <stdio.h>

#define FS_MAX_NODES 64
#define FS_NAME_LEN 32
#define FS_DATA_SIZE (64 * 1024)
#define FS_PATH_LEN 96

typedef struct {
    int used;
    char name[FS_NAME_LEN];
    int is_dir;
    int parent;
    int child;
    int next;
    int data_off;
    int size;
} fsnode_t;

static fsnode_t nodes[FS_MAX_NODES];
static char datapool[FS_DATA_SIZE];
static int data_used = 0;
static char cwd[FS_PATH_LEN];

static int data_alloc(int n) {
    if (n <= 0) return 0;
    if (data_used + n > FS_DATA_SIZE) return -1;
    int off = data_used;
    data_used += n;
    return off;
}

static int node_alloc(void) {
    for (int i = 0; i < FS_MAX_NODES; i++)
        if (!nodes[i].used) {
            nodes[i].used = 1;
            nodes[i].name[0] = 0;
            nodes[i].is_dir = 0;
            nodes[i].parent = -1;
            nodes[i].child = -1;
            nodes[i].next = -1;
            nodes[i].data_off = 0;
            nodes[i].size = 0;
            return i;
        }
    return -1;
}

static void path_abs(const char *cwdp, const char *path, char *out) {
    char tmp[FS_PATH_LEN * 2];
    int i = 0;
    if (path[0] != '/') {
        int j = 0;
        while (cwdp[j] && i < (int)sizeof(tmp) - 1) { tmp[i++] = cwdp[j++]; }
        if (i > 0 && tmp[i - 1] != '/' && i < (int)sizeof(tmp) - 1) tmp[i++] = '/';
    }
    int j = 0;
    while (path[j] && i < (int)sizeof(tmp) - 1) { tmp[i++] = path[j++]; }
    tmp[i] = 0;
    char parts[16][FS_NAME_LEN];
    int nparts = 0;
    int k = 0;
    while (tmp[k]) {
        while (tmp[k] == '/') k++;
        if (!tmp[k]) break;
        int m = 0;
        while (tmp[k] && tmp[k] != '/' && m < FS_NAME_LEN - 1) parts[nparts][m++] = tmp[k++];
        parts[nparts][m] = 0;
        if (!strcmp(parts[nparts], ".")) continue;
        if (!strcmp(parts[nparts], "..")) {
            if (nparts > 0) nparts--;
            continue;
        }
        nparts++;
        if (nparts >= 16) break;
    }
    int o = 0;
    out[o++] = '/';
    for (int p = 0; p < nparts; p++) {
        int m = 0;
        while (parts[p][m] && o < FS_PATH_LEN - 2) out[o++] = parts[p][m++];
        out[o++] = '/';
    }
    if (o > 1 && out[o - 1] == '/') o--;
    out[o] = 0;
}

static int find_child(int dir, const char *name) {
    if (dir < 0) return -1;
    int c = nodes[dir].child;
    while (c >= 0) {
        if (!strcmp(nodes[c].name, name)) return c;
        c = nodes[c].next;
    }
    return -1;
}

static int find_path(const char *abspath) {
    if (!strcmp(abspath, "/")) return 0;
    int cur = 0;
    int i = 1;
    char comp[FS_NAME_LEN];
    while (abspath[i]) {
        int m = 0;
        while (abspath[i] && abspath[i] != '/' && m < FS_NAME_LEN - 1) comp[m++] = abspath[i++];
        comp[m] = 0;
        while (abspath[i] == '/') i++;
        if (!m) continue;
        cur = find_child(cur, comp);
        if (cur < 0) return -1;
    }
    return cur;
}

static int split_parent(const char *abspath, char *parent_out, char *name_out) {
    int len = strlen(abspath);
    if (len <= 1) return -1;
    int end = len;
    while (end > 1 && abspath[end - 1] == '/') end--;
    int slash = end - 1;
    while (slash > 0 && abspath[slash - 1] != '/') slash--;
    int nm = end - slash;
    if (nm <= 0 || nm >= FS_NAME_LEN) return -1;
    memcpy(name_out, abspath + slash, nm);
    name_out[nm] = 0;
    if (!strcmp(name_out, ".") || !strcmp(name_out, "..")) return -1;
    if (slash <= 1) {
        parent_out[0] = '/';
        parent_out[1] = 0;
    } else {
        if (slash - 1 >= FS_PATH_LEN) return -1;
        memcpy(parent_out, abspath, slash - 1);
        parent_out[slash - 1] = 0;
    }
    return 0;
}

void fs_init(void) {
    memset(nodes, 0, sizeof(nodes));
    memset(datapool, 0, sizeof(datapool));
    data_used = 0;
    strcpy(cwd, "/");
    nodes[0].used = 1;
    strcpy(nodes[0].name, "/");
    nodes[0].is_dir = 1;
    nodes[0].parent = -1;
    nodes[0].child = -1;
    nodes[0].next = -1;
    fs_mkdir("/scripts");
    fs_write("/readme.txt",
        "myos ram filesystem\n"
        "everything here lives in ram and is gone on reboot\n"
        "commands: ls lt cat mkdir cd code run\n"
        "try: run demo.ez\n", -1);
    fs_write("/demo.ez",
        "# demo script\n"
        "echo hello from script\n"
        "calc 6*7\n"
        "h(x) = sin(x)\n"
        "calc h(2)\n"
        "plot h(x) 0 6.28\n", -1);
    fs_write("/scripts/spec.ez",
        "# spectrum demo\n"
        "fft sin(x)\n"
        "freq\n", -1);
}

int fs_mkdir(const char *path) {
    char abs[FS_PATH_LEN], par[FS_PATH_LEN], name[FS_NAME_LEN];
    path_abs(cwd, path, abs);
    if (!strcmp(abs, "/")) return 0;
    if (split_parent(abs, par, name)) return -1;
    int pi = find_path(par);
    if (pi < 0 || !nodes[pi].is_dir) return -1;
    if (find_child(pi, name) >= 0) return -1;
    int ni = node_alloc();
    if (ni < 0) return -1;
    strcpy(nodes[ni].name, name);
    nodes[ni].is_dir = 1;
    nodes[ni].parent = pi;
    nodes[ni].next = nodes[pi].child;
    nodes[pi].child = ni;
    return 0;
}

int fs_write(const char *path, const char *data, int len) {
    char abs[FS_PATH_LEN], par[FS_PATH_LEN], name[FS_NAME_LEN];
    path_abs(cwd, path, abs);
    if (len < 0) len = strlen(data);
    int ni = find_path(abs);
    if (ni >= 0 && nodes[ni].is_dir) return -1;
    if (ni < 0) {
        if (split_parent(abs, par, name)) return -1;
        int pi = find_path(par);
        if (pi < 0 || !nodes[pi].is_dir) return -1;
        ni = node_alloc();
        if (ni < 0) return -1;
        strcpy(nodes[ni].name, name);
        nodes[ni].is_dir = 0;
        nodes[ni].parent = pi;
        nodes[ni].next = nodes[pi].child;
        nodes[pi].child = ni;
    }
    int off = data_alloc(len + 1);
    if (off < 0) return -1;
    memcpy(datapool + off, data, len);
    datapool[off + len] = 0;
    nodes[ni].data_off = off;
    nodes[ni].size = len;
    return 0;
}

int fs_read(const char *path, char *out, int cap) {
    char abs[FS_PATH_LEN];
    path_abs(cwd, path, abs);
    int ni = find_path(abs);
    if (ni < 0 || nodes[ni].is_dir) return -1;
    int n = nodes[ni].size;
    if (n > cap - 1) n = cap - 1;
    memcpy(out, datapool + nodes[ni].data_off, n);
    out[n] = 0;
    return n;
}

int fs_list(const char *path, char *out, int cap) {
    char abs[FS_PATH_LEN];
    path_abs(cwd, path, abs);
    int ni = find_path(abs);
    if (ni < 0 || !nodes[ni].is_dir) return -1;
    int o = 0, count = 0;
    int c = nodes[ni].child;
    while (c >= 0 && o < cap - 2) {
        int i = 0;
        while (nodes[c].name[i] && o < cap - 2) out[o++] = nodes[c].name[i++];
        if (nodes[c].is_dir && o < cap - 2) out[o++] = '/';
        if (o < cap - 2) out[o++] = '\n';
        count++;
        c = nodes[c].next;
    }
    out[o] = 0;
    return count;
}

int fs_isdir(const char *path) {
    char abs[FS_PATH_LEN];
    path_abs(cwd, path, abs);
    int ni = find_path(abs);
    return ni >= 0 && nodes[ni].is_dir;
}

int fs_exists(const char *path) {
    char abs[FS_PATH_LEN];
    path_abs(cwd, path, abs);
    return find_path(abs) >= 0;
}

const char *fs_cwd(void) {
    return cwd;
}

int fs_cd(const char *path) {
    char abs[FS_PATH_LEN];
    path_abs(cwd, path, abs);
    int ni = find_path(abs);
    if (ni < 0 || !nodes[ni].is_dir) return -1;
    strcpy(cwd, abs);
    return 0;
}
