#include "gloscope.h"

const char *VertexShaderCode = "#version 440 core\n"
		"layout(location =   0) in      float a_hpos;\n"
		"layout(location =   1) in      float a_vpos;\n"
		"layout(location = 100) out     vec2  v_pos;\n"
		"layout(location = 200) uniform vec4 u_color;\n"
		"void main() {\n"
		"  v_pos = vec2(a_hpos, a_vpos);\n"
		"  gl_Position.xy = v_pos * vec2(2,1) - vec2(1,0);\n"
		"  gl_Position.zw = vec2(0,1);\n"
		"}\n";


const char *FragmentShaderCode = "#version 440 core\n"
		"layout(location = 100) in      vec2 v_pos;\n"
		"layout(location = 200) uniform vec4 u_color;\n"
		"out vec3 color;\n"
		"void main() {\n"
		"  color = u_color.rgb * u_color.a;\n"
		"}\n";


void handleGlError() {
	GLenum err = glGetError();
	if (err != GL_NO_ERROR) {
		fprintf(stderr, "GL ERROR: %d\n", err);
		exit(1);
	}
}


void *notnull(void *ptr) {
	if (ptr == NULL) {
		fprintf(stderr, "Unexpected NULL pointer\n");
		exit(1);
	}
	return ptr;
}


int CheckShader(GLuint ShaderID) {
	GLint InfoLogLength;
	GLchar *InfoLog;
	GLint Result;

	// Check Fragment Shader
	glGetShaderiv(ShaderID, GL_COMPILE_STATUS, &Result);
	glGetShaderiv(ShaderID, GL_INFO_LOG_LENGTH, &InfoLogLength);
	if (InfoLogLength > 0) {
		printf("Log for shader %d (%d bytes)\n", ShaderID, InfoLogLength);
		InfoLog = notnull(malloc((size_t) InfoLogLength));
		glGetShaderInfoLog(ShaderID, InfoLogLength, NULL, InfoLog);
		printf("%s\n", InfoLog);
		free(InfoLog);
	}

	if (Result == GL_TRUE) {
		return 1;
	} else {
		exit(1);
		return 0;
	}
}


int CheckProgram(GLuint ProgramID) {
	GLint InfoLogLength;
	GLchar *InfoLog;
	GLint Result;

	// Check Fragment Shader
	glGetProgramiv(ProgramID, GL_LINK_STATUS, &Result);
	glGetProgramiv(ProgramID, GL_INFO_LOG_LENGTH, &InfoLogLength);
	if (InfoLogLength > 0) {
		printf("Log for program %d (%d bytes)\n", ProgramID, InfoLogLength);
		InfoLog = notnull(malloc((size_t) InfoLogLength));
		glGetProgramInfoLog(ProgramID, InfoLogLength, NULL, InfoLog);
		printf("%s\n", InfoLog);
		free(InfoLog);
	}

	if (Result == GL_TRUE) {
		return 1;
	} else {
		exit(1);
		return 0;
	}
}


GLuint LoadShaders() {
	GLuint VertexShaderID;
	GLuint FragmentShaderID;
	GLuint ProgramID;

	// Create the shaders
	VertexShaderID = glCreateShader(GL_VERTEX_SHADER);
	FragmentShaderID = glCreateShader(GL_FRAGMENT_SHADER);

	// Compile Vertex Shader
	printf("Compiling shader\n");
	glShaderSource(VertexShaderID, 1, &VertexShaderCode , NULL);
	glCompileShader(VertexShaderID);
	CheckShader(VertexShaderID);

	// Compile Fragment Shader
	printf("Compiling shader\n");
	glShaderSource(FragmentShaderID, 1, &FragmentShaderCode, NULL);
	glCompileShader(FragmentShaderID);
	CheckShader(FragmentShaderID);

	// Link the program
	printf("Linking program\n");
	ProgramID = glCreateProgram();
	glAttachShader(ProgramID, VertexShaderID);
	glAttachShader(ProgramID, FragmentShaderID);
	glLinkProgram(ProgramID);
	CheckProgram(ProgramID);

	glDetachShader(ProgramID, VertexShaderID);
	glDetachShader(ProgramID, FragmentShaderID);
	
	glDeleteShader(VertexShaderID);
	glDeleteShader(FragmentShaderID);

	return ProgramID;
}


void gloscope_reshape(struct gloscope_context *ctx, int num_channels
		, int num_samples) {
	struct gloscope_color default_colors[16] = {
			{1,1,1,1},{1,0,0,1},{0,1,0,1},{0,0,1,1},
			{1,1,0,1},{1,0,1,1},{0,1,1,1},{.75,.75,.75,1},
			{1,.5,0,1},{1,0,.5,1},{0,1,.5,1},{.75,.75,.75,1},
			{.5,1,0,1},{.5,0,1,1},{0,.5,1,1},{.5,.5,.5,1},
	};

	if (ctx->num_samples != 0 && ctx->num_channels != 0) {
		for (int i = 0; i < num_channels; i++) {
			free(ctx->horz_data[i]);
			free(ctx->vert_data[i]);
		}
		free(ctx->horz_data);
		free(ctx->vert_data);
		free(ctx->colors);
	}

	if (num_channels != 0 && num_samples != 0) {
		ctx->num_samples = num_samples;
		ctx->num_channels = num_channels;
		ctx->colors = notnull(malloc(num_channels * sizeof(ctx->colors[0])));
		ctx->horz_data = notnull(malloc(num_channels * sizeof(void *)));
		ctx->vert_data = notnull(malloc(num_channels * sizeof(void *)));

		for (int i = 0; i < num_channels; i++) {
			ctx->horz_data[i] = notnull(malloc(num_samples * sizeof(sample_t)));
			ctx->vert_data[i] = notnull(malloc(num_samples * sizeof(sample_t)));
		}

		GLfloat horzScale = 1.f / (num_samples-1);
		for (int i = 0; i < num_channels; i++) {
			ctx->colors[i] = default_colors[i % 16];
			for (int j = 0; j < num_samples; j++) {
				ctx->horz_data[i][j] = j * horzScale;
				ctx->vert_data[i][j] = 0;
			}
		}
	}
}


int gloscope_init(struct gloscope_context *ctx,
		int num_channels, int num_samples) {
	memset(ctx, 0, sizeof(*ctx));
	GLuint VertexArrayID1;
	GLuint VertexArrayID;

	// Initialise GLFW
	if(!glfwInit())
	{
		fprintf( stderr, "Failed to initialize GLFW\n" );
		return -1;
	}

	gloscope_reshape(ctx, num_channels, num_samples);

	glfwWindowHint(GLFW_SAMPLES, 4); // 4x antialiasing
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4); // We want OpenGL 4.4
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 4);
	// To make MacOS happy; should not be needed
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	// We don't want the old OpenGL 
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	// Open a window and create its OpenGL context
	ctx->window = notnull(glfwCreateWindow( 1024, 768, "gloscope", NULL, NULL));
	glfwMakeContextCurrent(ctx->window); // Initialize GLEW
	//glewExperimental=1; // Needed in core profile
	if (glewInit() != GLEW_OK) {
		fprintf(stderr, "Failed to initialize GLEW\n");
		return -1;
	}

	glGenVertexArrays(1, &VertexArrayID);
	glBindVertexArray(VertexArrayID);
	glGenBuffers(1, &ctx->horz_vbo);

	glGenVertexArrays(1, &VertexArrayID1);
	glBindVertexArray(VertexArrayID1);
	glGenBuffers(1, &ctx->vert_vbo);

	// Create shader program
	ctx->programID = LoadShaders();

	// Ensure we can capture the escape key being pressed below
	glfwSetInputMode(ctx->window, GLFW_STICKY_KEYS, GL_TRUE);
	ctx->ready = 1;
	
	return 1;
}

int gloscope_render(struct gloscope_context *ctx) {
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glUseProgram(ctx->programID);

	int stop_idx = ctx->stop_idx;
	if (stop_idx >= ctx->num_samples)
		stop_idx = ctx->num_samples-1;
	int num_samples = 1 + stop_idx - ctx->start_idx;
	GLsizeiptr size = num_samples * sizeof(sample_t);
	GLsizeiptr offset = (ctx->start_idx) * sizeof(sample_t);

	for (int c = 0; c < ctx->num_channels; c++) {

		struct gloscope_color *color = &ctx->colors[c];
		glUniform4f(200, color->r, color->g, color->b, color->a);

		glEnableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, ctx->horz_vbo);
		glBufferData(GL_ARRAY_BUFFER, size,
				ctx->horz_data[c] + offset, GL_STATIC_DRAW);
		glVertexAttribPointer(0, 1, GL_FLOAT, GL_FALSE, 0, (void*)0);

		glEnableVertexAttribArray(1);
		glBindBuffer(GL_ARRAY_BUFFER, ctx->vert_vbo);
		glBufferData(GL_ARRAY_BUFFER, size,
				ctx->vert_data[c] + offset, GL_STATIC_DRAW);
		glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 0, (void*)0);

		glDrawArrays(GL_LINE_STRIP, 0, ctx->num_samples);

		glDisableVertexAttribArray(0);
		glDisableVertexAttribArray(1);
	}

	// Swap buffers
	glfwSwapBuffers(ctx->window);
	glfwPollEvents();
	handleGlError();

	// Check if the ESC key was pressed or the window was closed
	int shouldClose = glfwWindowShouldClose(ctx->window);
	int isEscapePressed = glfwGetKey(ctx->window, GLFW_KEY_ESCAPE) == GLFW_PRESS;

	return shouldClose || isEscapePressed;
}

