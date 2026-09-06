#include <gl.h>
#include <gpu.h>

#define STACK_DEPTH 8
#define VBUF_MAX 2048
#define PI 3.14159265f

typedef struct { GLfloat x, y, z, r, g, b; } Vert;

static GLfloat mModel[STACK_DEPTH][16];
static GLfloat mProj[STACK_DEPTH][16];
static int spModel = 0;
static int spProj = 0;
static GLenum curMode = GL_MODELVIEW;

static GLfloat clearR, clearG, clearB;
static int depthOn = 0;

static int vpX, vpY, vpW, vpH;
static int fbW, fbH, fbStride;
static int orgX, orgY;
static uint32_t *cbuf;
static float zbuf[1280 * 720];

static Vert vbuf[VBUF_MAX];
static int vcount = 0;
static GLenum primMode = GL_TRIANGLES;
static GLfloat curR = 1, curG = 1, curB = 1;

static float fsqrt_f(float x) {
    float r;
    __asm__ volatile("fsqrt" : "=t"(r) : "0"(x));
    return r;
}

static void fsincos_f(float a, float *s, float *c) {
    __asm__ volatile("fsincos" : "=t"(*c), "=u"(*s) : "0"(a));
}

static GLfloat (*curStack(void))[16] {
    return curMode == GL_PROJECTION ? mProj : mModel;
}

static int *curSp(void) {
    return curMode == GL_PROJECTION ? &spProj : &spModel;
}

static void matMul(GLfloat *o, const GLfloat *a, const GLfloat *b) {
    GLfloat t[16];
    for (int c = 0; c < 4; c++)
        for (int r = 0; r < 4; r++)
            t[c * 4 + r] = a[r] * b[c * 4] + a[4 + r] * b[c * 4 + 1] +
                           a[8 + r] * b[c * 4 + 2] + a[12 + r] * b[c * 4 + 3];
    for (int i = 0; i < 16; i++) o[i] = t[i];
}

static void matVec(GLfloat *o, const GLfloat *m, const GLfloat *v) {
    for (int r = 0; r < 4; r++)
        o[r] = m[r] * v[0] + m[4 + r] * v[1] + m[8 + r] * v[2] + m[12 + r] * v[3];
}

void gl_init(int x, int y, int w, int h) {
    orgX = x;
    orgY = y;
    fbW = w;
    fbH = h;
    fbStride = (int)gpu_stride();
    cbuf = gpu_buffer();
    glViewport(0, 0, w, h);
    curMode = GL_MODELVIEW;
    spModel = spProj = 0;
    glLoadIdentity();
    curMode = GL_PROJECTION;
    glLoadIdentity();
    curMode = GL_MODELVIEW;
    clearR = clearG = clearB = 0;
    depthOn = 0;
}

void glViewport(GLint x, GLint y, GLint w, GLint h) {
    vpX = x;
    vpY = y;
    vpW = w;
    vpH = h;
}

void glMatrixMode(GLenum mode) {
    if (mode == GL_MODELVIEW || mode == GL_PROJECTION) curMode = mode;
}

void glLoadIdentity(void) {
    GLfloat(*s)[16] = curStack();
    int sp = *curSp();
    for (int i = 0; i < 16; i++) s[sp][i] = (i % 5 == 0) ? 1.0f : 0.0f;
}

void glPushMatrix(void) {
    int *sp = curSp();
    if (*sp + 1 >= STACK_DEPTH) return;
    GLfloat(*s)[16] = curStack();
    for (int i = 0; i < 16; i++) s[*sp + 1][i] = s[*sp][i];
    (*sp)++;
}

void glPopMatrix(void) {
    int *sp = curSp();
    if (*sp > 0) (*sp)--;
}

void glTranslatef(GLfloat x, GLfloat y, GLfloat z) {
    GLfloat t[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, x, y, z, 1};
    GLfloat(*s)[16] = curStack();
    matMul(s[*curSp()], s[*curSp()], t);
}

void glScalef(GLfloat x, GLfloat y, GLfloat z) {
    GLfloat t[16] = {x, 0, 0, 0, 0, y, 0, 0, 0, 0, z, 0, 0, 0, 0, 1};
    GLfloat(*s)[16] = curStack();
    matMul(s[*curSp()], s[*curSp()], t);
}

void glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z) {
    GLfloat len = fsqrt_f(x * x + y * y + z * z);
    if (len == 0.0f) return;
    x /= len;
    y /= len;
    z /= len;
    float s, c;
    fsincos_f(angle * (PI / 180.0f), &s, &c);
    GLfloat t = 1.0f - c;
    GLfloat r[16] = {
        t * x * x + c, t * x * y + s * z, t * x * z - s * y, 0,
        t * x * y - s * z, t * y * y + c, t * y * z + s * x, 0,
        t * x * z + s * y, t * y * z - s * x, t * z * z + c, 0,
        0, 0, 0, 1};
    GLfloat(*st)[16] = curStack();
    matMul(st[*curSp()], st[*curSp()], r);
}

void glFrustumf(GLfloat l, GLfloat r, GLfloat b, GLfloat t, GLfloat n, GLfloat f) {
    GLfloat m[16] = {0};
    m[0] = 2 * n / (r - l);
    m[5] = 2 * n / (t - b);
    m[8] = (r + l) / (r - l);
    m[9] = (t + b) / (t - b);
    m[10] = -(f + n) / (f - n);
    m[11] = -1;
    m[14] = -2 * f * n / (f - n);
    GLfloat(*s)[16] = curStack();
    matMul(s[*curSp()], s[*curSp()], m);
}

void gluPerspective(GLfloat fovy, GLfloat aspect, GLfloat n, GLfloat f) {
    float s, c;
    fsincos_f((fovy * 0.5f) * (PI / 180.0f), &s, &c);
    GLfloat t = c / s;
    glFrustumf(-t * aspect * n, t * aspect * n, -t * n, t * n, n, f);
}

void glClearColor(GLfloat r, GLfloat g, GLfloat b) {
    clearR = r;
    clearG = g;
    clearB = b;
}

void glClear(GLbitfield mask) {
    uint32_t cc = ((uint32_t)(clearR * 255) << 16) |
                  ((uint32_t)(clearG * 255) << 8) |
                  (uint32_t)(clearB * 255);
    if (mask & GL_COLOR_BUFFER_BIT)
        for (int y = 0; y < fbH; y++)
            for (int x = 0; x < fbW; x++)
                cbuf[(y + orgY) * fbStride + (x + orgX)] = cc;
    if (mask & GL_DEPTH_BUFFER_BIT) {
        int n = fbW * fbH;
        if (n > 1280 * 720) n = 1280 * 720;
        for (int i = 0; i < n; i++) zbuf[i] = 1.0f;
    }
}

void glEnable(GLenum cap) {
    if (cap == GL_DEPTH_TEST) depthOn = 1;
}

void glDisable(GLenum cap) {
    if (cap == GL_DEPTH_TEST) depthOn = 0;
}

void glBegin(GLenum mode) {
    primMode = mode;
    vcount = 0;
}

void glColor3f(GLfloat r, GLfloat g, GLfloat b) {
    curR = r;
    curG = g;
    curB = b;
}

static void pushVert(GLfloat x, GLfloat y, GLfloat z) {
    if (vcount >= VBUF_MAX) return;
    GLfloat v[4] = {x, y, z, 1};
    GLfloat eye[4], clip[4];
    matVec(eye, mModel[spModel], v);
    matVec(clip, mProj[spProj], eye);
    if (clip[3] == 0.0f) return;
    GLfloat nx = clip[0] / clip[3];
    GLfloat ny = clip[1] / clip[3];
    GLfloat nz = clip[2] / clip[3];
    Vert *o = &vbuf[vcount++];
    o->x = vpX + (nx * 0.5f + 0.5f) * vpW;
    o->y = vpY + (0.5f - ny * 0.5f) * vpH;
    o->z = nz * 0.5f + 0.5f;
    o->r = curR;
    o->g = curG;
    o->b = curB;
}

void glVertex3f(GLfloat x, GLfloat y, GLfloat z) {
    pushVert(x, y, z);
}

void glVertex2f(GLfloat x, GLfloat y) {
    pushVert(x, y, 0);
}

static void putPx(int x, int y, float d, float r, float g, float b) {
    if (x < 0 || y < 0 || x >= fbW || y >= fbH) return;
    int idx = (y + orgY) * fbStride + (x + orgX);
    if (depthOn && d >= zbuf[y * fbW + x]) return;
    if (depthOn) zbuf[y * fbW + x] = d;
    int ri = (int)(r * 255);
    int gi = (int)(g * 255);
    int bi = (int)(b * 255);
    if (ri < 0) ri = 0;
    if (ri > 255) ri = 255;
    if (gi < 0) gi = 0;
    if (gi > 255) gi = 255;
    if (bi < 0) bi = 0;
    if (bi > 255) bi = 255;
    cbuf[idx] = ((uint32_t)ri << 16) | ((uint32_t)gi << 8) | (uint32_t)bi;
}

static void tri(const Vert *a, const Vert *b, const Vert *c) {
    float x0 = a->x, y0 = a->y, x1 = b->x, y1 = b->y, x2 = c->x, y2 = c->y;
    int minx = (int)(x0 < x1 ? (x0 < x2 ? x0 : x2) : (x1 < x2 ? x1 : x2));
    int maxx = (int)(x0 > x1 ? (x0 > x2 ? x0 : x2) : (x1 > x2 ? x1 : x2)) + 1;
    int miny = (int)(y0 < y1 ? (y0 < y2 ? y0 : y2) : (y1 < y2 ? y1 : y2));
    int maxy = (int)(y0 > y1 ? (y0 > y2 ? y0 : y2) : (y1 > y2 ? y1 : y2)) + 1;
    if (minx < 0) minx = 0;
    if (miny < 0) miny = 0;
    if (maxx >= fbW) maxx = fbW - 1;
    if (maxy >= fbH) maxy = fbH - 1;
    float area = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0);
    if (area == 0.0f) return;
    for (int y = miny; y <= maxy; y++) {
        for (int x = minx; x <= maxx; x++) {
            float px = (float)x + 0.5f, py = (float)y + 0.5f;
            float w0 = ((x1 - px) * (y2 - py) - (x2 - px) * (y1 - py)) / area;
            float w1 = ((x2 - px) * (y0 - py) - (x0 - px) * (y2 - py)) / area;
            float w2 = 1.0f - w0 - w1;
            if ((w0 >= 0 && w1 >= 0 && w2 >= 0) || (w0 <= 0 && w1 <= 0 && w2 <= 0)) {
                float d = w0 * a->z + w1 * b->z + w2 * c->z;
                float r = w0 * a->r + w1 * b->r + w2 * c->r;
                float g = w0 * a->g + w1 * b->g + w2 * c->g;
                float bl = w0 * a->b + w1 * b->b + w2 * c->b;
                putPx(x, y, d, r, g, bl);
            }
        }
    }
}

static void seg(const Vert *a, const Vert *b) {
    int x0 = (int)a->x, y0 = (int)a->y, x1 = (int)b->x, y1 = (int)b->y;
    int dx = x1 >= x0 ? x1 - x0 : x0 - x1;
    int dy = y1 >= y0 ? y1 - y0 : y0 - y1;
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;
    for (;;) {
        putPx(x0, y0, 0, a->r, a->g, a->b);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx) { err += dx; y0 += sy; }
    }
}

void glEnd(void) {
    if (primMode == GL_TRIANGLES) {
        for (int i = 0; i + 2 < vcount; i += 3)
            tri(&vbuf[i], &vbuf[i + 1], &vbuf[i + 2]);
    } else if (primMode == GL_QUADS) {
        for (int i = 0; i + 3 < vcount; i += 4) {
            tri(&vbuf[i], &vbuf[i + 1], &vbuf[i + 2]);
            tri(&vbuf[i], &vbuf[i + 2], &vbuf[i + 3]);
        }
    } else if (primMode == GL_LINES) {
        for (int i = 0; i + 1 < vcount; i += 2)
            seg(&vbuf[i], &vbuf[i + 1]);
    }
    vcount = 0;
}

void glFlush(void) {}
