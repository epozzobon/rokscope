#include <stdio.h>
#include <stdlib.h>
#include <memory.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

typedef GLfloat sample_t;

struct gloscope_color {
	GLfloat r;
	GLfloat g;
	GLfloat b;
	GLfloat a;
};

struct gloscope_plot {
	GLuint num_samples;
	struct gloscope_color color;
	sample_t *vert_data;
	sample_t *horz_data;
};

struct gloscope_private {
	GLuint programID;
	GLuint horz_vbo;
	GLuint vert_vbo;
	GLFWwindow* window;
};

struct gloscope_context {
	struct gloscope_private _p;
	int num_channels;
	int start_idx;
	int stop_idx;
	int ready;
	struct gloscope_plot **plots;
};

int gloscope_init(struct gloscope_context *, int, GLuint);
int gloscope_render(struct gloscope_context *);
void gloscope_reshape(struct gloscope_context *, int, GLuint);
void *notnull(void *);
void *zalloc(size_t);

