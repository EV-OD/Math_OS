#include <gl.h>
#include <gpu.h>
#include <stdio.h>
#include <process.h>
#include <string.h>

static const float verts[8][3] = {
    {-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1},
    {-1, -1, 1}, {1, -1, 1}, {1, 1, 1}, {-1, 1, 1},
};

static const int faces[6][4] = {
    {0, 1, 2, 3}, {4, 5, 6, 7}, {0, 1, 5, 4},
    {2, 3, 7, 6}, {0, 3, 7, 4}, {1, 2, 6, 5},
};

static const float fcols[6][3] = {
    {1, 0.2f, 0.2f}, {0.2f, 1, 0.3f}, {0.3f, 0.5f, 1},
    {1, 0.85f, 0.2f}, {0.8f, 0.4f, 1}, {0.25f, 0.9f, 0.9f},
};

void prog_glcube(void) {
    if (gpu_claim() != 0) {
        printf("glcube: GPU screen busy\n");
        proc_exit();
    }
    printf("glcube running on GPU screen (ctrl+c stops)\n");
    uint32_t w = gpu_width();
    uint32_t h = gpu_height();
    gl_init(gpu_vx(), gpu_vy(), (int)w, (int)h);
    glEnable(GL_DEPTH_TEST);
    float ang = 0;
    uint32_t frame = 0;
    for (;;) {
        glClearColor(0.03f, 0.05f, 0.12f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluPerspective(60.0f, (GLfloat)w / (GLfloat)h, 0.1f, 100.0f);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glTranslatef(0, 0, -5.0f);
        glRotatef(ang, 0, 1, 0);
        glRotatef(ang * 0.7f, 1, 0, 0);
        glBegin(GL_QUADS);
        for (int f = 0; f < 6; f++) {
            glColor3f(fcols[f][0], fcols[f][1], fcols[f][2]);
            for (int v = 0; v < 4; v++) {
                int i = faces[f][v];
                glVertex3f(verts[i][0], verts[i][1], verts[i][2]);
            }
        }
        glEnd();
        gpu_present();
        ang += 6.0f;
        if (ang >= 360.0f) ang -= 360.0f;
        frame++;
        proc_sleep(3);
    }
}
