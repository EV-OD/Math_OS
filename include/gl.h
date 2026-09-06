#ifndef TINYGL_H
#define TINYGL_H

typedef unsigned int GLenum;
typedef unsigned char GLboolean;
typedef unsigned int GLbitfield;
typedef float GLfloat;
typedef int GLint;

#define GL_FALSE 0
#define GL_TRUE 1

#define GL_TRIANGLES 0x0004
#define GL_QUADS 0x0007
#define GL_LINES 0x0001

#define GL_MODELVIEW 0x1700
#define GL_PROJECTION 0x1701

#define GL_COLOR_BUFFER_BIT 0x4000
#define GL_DEPTH_BUFFER_BIT 0x0100
#define GL_DEPTH_TEST 0x0B71

void gl_init(int x, int y, int w, int h);
void glViewport(GLint x, GLint y, GLint w, GLint h);
void glMatrixMode(GLenum mode);
void glLoadIdentity(void);
void glPushMatrix(void);
void glPopMatrix(void);
void glTranslatef(GLfloat x, GLfloat y, GLfloat z);
void glScalef(GLfloat x, GLfloat y, GLfloat z);
void glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z);
void glFrustumf(GLfloat l, GLfloat r, GLfloat b, GLfloat t, GLfloat n, GLfloat f);
void gluPerspective(GLfloat fovy, GLfloat aspect, GLfloat n, GLfloat f);
void glClearColor(GLfloat r, GLfloat g, GLfloat b);
void glClear(GLbitfield mask);
void glEnable(GLenum cap);
void glDisable(GLenum cap);
void glBegin(GLenum mode);
void glColor3f(GLfloat r, GLfloat g, GLfloat b);
void glVertex3f(GLfloat x, GLfloat y, GLfloat z);
void glVertex2f(GLfloat x, GLfloat y);
void glEnd(void);
void glFlush(void);

#endif
