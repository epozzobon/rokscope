#include <stdio.h>
#include <stdlib.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#define POINTS_COUNT 256

struct gloscope_context {
	GLuint programID;
	GLuint horz_vbo;
	GLuint vert_vbo;
	GLFWwindow* window;
	GLfloat horz_data[POINTS_COUNT];
	GLfloat vert_data[POINTS_COUNT];
	int num_samples;
};

int gloscope_init(struct gloscope_context *);
int gloscope_render(struct gloscope_context *);

