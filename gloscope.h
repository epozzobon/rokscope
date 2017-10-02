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

struct gloscope_context {
	int num_samples;
	int num_channels;
	int start_idx;
	int stop_idx;
	int ready;
	GLuint programID;
	GLuint horz_vbo;
	GLuint vert_vbo;
	GLFWwindow* window;
	sample_t **horz_data;
	sample_t **vert_data;
	struct gloscope_color *colors;
};

int gloscope_init(struct gloscope_context *, int, int);
int gloscope_render(struct gloscope_context *);
void gloscope_reshape(struct gloscope_context *, int, int);
void *notnull(void *);

