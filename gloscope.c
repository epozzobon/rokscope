#include "gloscope.h"

const char *VertexShaderCode = "#version 440 core\n"
		"layout(location =  0) in  float a_hpos;\n"
		"layout(location =  1) in  float a_vpos;\n"
		"layout(location = 10) out vec2 v_pos;\n"
		"void main() {\n"
		"  v_pos = vec2(a_hpos, a_vpos);\n"
		"  gl_Position.xy = v_pos * 2 - vec2(1,1);\n"
		"  gl_Position.zw = vec2(0,1);\n"
		"}\n";


const char *FragmentShaderCode = "#version 440 core\n"
		"layout(location = 10) in vec2 v_pos;\n"
		"out vec3 color;\n"
		"void main() {\n"
		"  color = vec3(0,1,0);\n"
		"}\n";


void handleGlError() {
	GLenum err = glGetError();
	if (err != GL_NO_ERROR) {
		fprintf(stderr, "GL ERROR: %d\n", err);
		exit(1);
	}
}


int CheckShader(GLuint ShaderID) {
	int InfoLogLength;
	GLchar *InfoLog;
	GLint Result;

	// Check Fragment Shader
	glGetShaderiv(ShaderID, GL_COMPILE_STATUS, &Result);
	glGetShaderiv(ShaderID, GL_INFO_LOG_LENGTH, &InfoLogLength);
	if (InfoLogLength > 0) {
		printf("Log for shader %d (%d bytes)\n", ShaderID, InfoLogLength);
		InfoLog = malloc(InfoLogLength);
		if (InfoLog == NULL) {
			exit(1);
			return 0;
		}
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
	int InfoLogLength;
	GLchar *InfoLog;
	GLint Result;

	// Check Fragment Shader
	glGetProgramiv(ProgramID, GL_LINK_STATUS, &Result);
	glGetProgramiv(ProgramID, GL_INFO_LOG_LENGTH, &InfoLogLength);
	if (InfoLogLength > 0) {
		printf("Log for program %d (%d bytes)\n", ProgramID, InfoLogLength);
		InfoLog = malloc(InfoLogLength);
		if (InfoLog == NULL) {
			exit(1);
			return 0;
		}
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


int gloscope_init(struct gloscope_context *ctx) {
	GLuint VertexArrayID1;
	GLuint VertexArrayID;

	// Initialise GLFW
	if(!glfwInit())
	{
		fprintf( stderr, "Failed to initialize GLFW\n" );
		return -1;
	}

	ctx->num_samples = POINTS_COUNT;
	for (int i = 0; i < POINTS_COUNT; i++) {
		ctx->horz_data[i] = (float) i / (float) (POINTS_COUNT-1);
	}

	//glfwWindowHint(GLFW_SAMPLES, 4); // 4x antialiasing
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4); // We want OpenGL 4.4
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 4);
	// To make MacOS happy; should not be needed
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	// We don't want the old OpenGL 
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	// Open a window and create its OpenGL context
	ctx->window = glfwCreateWindow( 1024, 768, "Tutorial 01", NULL, NULL);
	if (ctx->window == NULL) {
		fprintf( stderr, "Failed to open GLFW window. If you have an Intel GPU, they are not 4.4 compatible.\n" );
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(ctx->window); // Initialize GLEW
	//glewExperimental=1; // Needed in core profile
	if (glewInit() != GLEW_OK) {
		fprintf(stderr, "Failed to initialize GLEW\n");
		return -1;
	}

	// Wireframe mode
	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	// Push vertex buffer
	glGenVertexArrays(1, &VertexArrayID);
	glBindVertexArray(VertexArrayID);
	glGenBuffers(1, &ctx->horz_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, ctx->horz_vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(ctx->horz_data),
			ctx->horz_data, GL_STATIC_DRAW);

	glGenVertexArrays(1, &VertexArrayID1);
	glBindVertexArray(VertexArrayID1);
	glGenBuffers(1, &ctx->vert_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, ctx->vert_vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(ctx->vert_data),
			ctx->vert_data, GL_STATIC_DRAW);

	// Create shader program
	ctx->programID = LoadShaders();

	// Ensure we can capture the escape key being pressed below
	glfwSetInputMode(ctx->window, GLFW_STICKY_KEYS, GL_TRUE);
	
	return 1;
}

int gloscope_render(struct gloscope_context *ctx) {
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glUseProgram(ctx->programID);

	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, ctx->horz_vbo);
	glVertexAttribPointer(0, 1, GL_FLOAT, GL_FALSE, 0, (void*)0);

	glEnableVertexAttribArray(1);
	glBindBuffer(GL_ARRAY_BUFFER, ctx->vert_vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(ctx->vert_data),
			ctx->vert_data, GL_STATIC_DRAW);
	glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 0, (void*)0);

	// Draw the triangle !
	glDrawArrays(GL_LINE_STRIP, 0, POINTS_COUNT);
	glDisableVertexAttribArray(0);
	glDisableVertexAttribArray(1);

	// Swap buffers
	glfwSwapBuffers(ctx->window);
	glfwPollEvents();

	// Check if the ESC key was pressed or the window was closed
	return glfwGetKey(ctx->window, GLFW_KEY_ESCAPE) == GLFW_PRESS
			|| glfwWindowShouldClose(ctx->window);
}


#ifdef TEST

int main() {
	struct gloscope_context ctx;	

	gloscope_init(&ctx);
	int t = 0;

	int exit;
	do {
		for (int i = 0; i < POINTS_COUNT; i++) {
			ctx->vert_data[(i+t) % POINTS_COUNT] =
					(float) (i*4 % POINTS_COUNT) / (float) (POINTS_COUNT-1);
		}
		t++;
		exit = gloscope_render(&ctx);
	} while(!exit);
}	
	
#endif	

