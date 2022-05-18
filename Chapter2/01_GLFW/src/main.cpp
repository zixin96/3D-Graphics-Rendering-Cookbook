#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <stdio.h>
#include <stdlib.h>


//  Let's draw a colored triangle
// For the sake of brevity, all error checking is omitted 

// We use the GLSL built-in gl_VertexID input variable to index into the pos[]
// and col[] arrays to generate the vertex positions and colors programmatically
// In this case, no user-defined inputs to the vertex shader are required.
static const char* shaderCodeVertex = R"(
#version 460 core
layout (location=0) out vec3 color;
const vec2 pos[3] = vec2[3](
	vec2(-0.6, -0.4),
	vec2( 0.6, -0.4),
	vec2( 0.0,  0.6)
);
const vec3 col[3] = vec3[3](
	vec3( 1.0, 0.0, 0.0 ),
	vec3( 0.0, 1.0, 0.0 ),
	vec3( 0.0, 0.0, 1.0 )
);
void main()
{
	gl_Position = vec4(pos[gl_VertexID], 0.0, 1.0);
	color = col[gl_VertexID];
}
)";

static const char* shaderCodeFragment = R"(
#version 460 core
layout (location=0) in vec3 color;
layout (location=0) out vec4 out_FragColor;
void main()
{
	out_FragColor = vec4(color, 1.0);
};
)";

int main(void)
{
	// we set the GLFW error callback via a simple lambda to catch potential errors
	glfwSetErrorCallback(
		[](int error, const char* description)
		{
			fprintf(stderr, "Error: %s\n", description);
		}
	);

	//  try to initialize GLFW
	if (!glfwInit())
		exit(EXIT_FAILURE);

	// tell GLFW which version of OpenGL we want to use
	// we will use OpenGL 4.6 Core Profile
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	GLFWwindow* window = glfwCreateWindow(1024, 768, "Simple example", nullptr, nullptr);
	if (!window)
	{
		glfwTerminate();
		exit(EXIT_FAILURE);
	}

	//  set a callback for key events
	glfwSetKeyCallback(
		window,
		[](GLFWwindow* window, int key, int scancode, int action, int mods)
		{
			if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
				glfwSetWindowShouldClose(window, GLFW_TRUE);
		}
	);

	// prepare the OpenGL context
	glfwMakeContextCurrent(window);
	//  use the GLAD library to import all OpenGL entry pointsand extensions
	gladLoadGL(glfwGetProcAddress);
	glfwSwapInterval(1);

	// Both shaders should be compiled and linked to a shader program
	const GLuint shaderVertex = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(shaderVertex, 1, &shaderCodeVertex, nullptr);
	glCompileShader(shaderVertex);

	const GLuint shaderFragment = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(shaderFragment, 1, &shaderCodeFragment, nullptr);
	glCompileShader(shaderFragment);

	const GLuint program = glCreateProgram();
	glAttachShader(program, shaderVertex);
	glAttachShader(program, shaderFragment);

	glLinkProgram(program);
	glUseProgram(program);

	// create a VAO
	// For this example, we will use the vertex shader to generate
	// all vertex data, so an empty VAO will be sufficient
	GLuint vao;
	glCreateVertexArrays(1, &vao);
	glBindVertexArray(vao);

	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);

	// The main loop starts by checking whether the window should be closed
	while (!glfwWindowShouldClose(window))
	{
		// Implement a resizable window by reading the current width and height from GLFW
		// and updating the OpenGL viewport accordingly
		int width, height;
		glfwGetFramebufferSize(window, &width, &height);
		glViewport(0, 0, width, height);

		// Clear the screen
		glClear(GL_COLOR_BUFFER_BIT);

		// render the triangle
		// The glDrawArrays() function can be invoked with the empty VAO that we bound earlier
		glDrawArrays(GL_TRIANGLES, 0, 3);

		// The fragment shader output was rendered into the back buffer. Let's swap the front
		// and back buffers to make the triangle visible
		glfwSwapBuffers(window);

		// poll the events
		glfwPollEvents();
	}

	//  delete the OpenGL objects that we created
	glDeleteProgram(program);
	glDeleteShader(shaderFragment);
	glDeleteShader(shaderVertex);
	glDeleteVertexArrays(1, &vao);

	// terminate GLFW
	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}
