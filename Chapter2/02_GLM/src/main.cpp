#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/ext.hpp>

#include <stdio.h>
#include <stdlib.h>

using glm::mat4;
using glm::vec3;

// let's draw a rotating 3D cube with wireframe contours
// TODO: See 3D GRC Page 35: we can make the uniform buffer twice as large and store two different copies of PerFrameData. This way, we can update the entire buffer with just one call to glNamedBufferSubData(). To do this, we need to use the offset parameter of glBindBufferRange() to feed the correct instance of PerFrameData into the shader

static const char* shaderCodeVertex = R"(
#version 460 core
// Observe that PerFrameData input structure reflects the PerFrameData structure in the C++ code
layout(std140, binding = 0) uniform PerFrameData
{
	uniform mat4 MVP;
	uniform int isWireframe;
};
layout (location=0) out vec3 color;

// We do not use normal vectors here, which means we can perfectly share 8 vertices among all the 6 adjacent faces of the cube
const vec3 pos[8] = vec3[8](
	vec3(-1.0,-1.0, 1.0),
	vec3( 1.0,-1.0, 1.0),
	vec3( 1.0, 1.0, 1.0),
	vec3(-1.0, 1.0, 1.0),

	vec3(-1.0,-1.0,-1.0),
	vec3( 1.0,-1.0,-1.0),
	vec3( 1.0, 1.0,-1.0),
	vec3(-1.0, 1.0,-1.0)
);
const vec3 col[8] = vec3[8](
	vec3( 1.0, 0.0, 0.0),
	vec3( 0.0, 1.0, 0.0),
	vec3( 0.0, 0.0, 1.0),
	vec3( 1.0, 1.0, 0.0),

	vec3( 1.0, 1.0, 0.0),
	vec3( 0.0, 0.0, 1.0),
	vec3( 0.0, 1.0, 0.0),
	vec3( 1.0, 0.0, 0.0)
);
// use indices to construct the actual cube faces
const int indices[36] = int[36](
	// front
	0, 1, 2, 2, 3, 0,
	// right
	1, 5, 6, 6, 2, 1,
	// back
	7, 6, 5, 5, 4, 7,
	// left
	4, 0, 3, 3, 7, 4,
	// bottom
	4, 5, 1, 1, 0, 4,
	// top
	3, 2, 6, 6, 7, 3
);
void main()
{
	// The gl_VertexID input variable is used to retrieve an index, which is used to get corresponding values for the position and color.
	int idx = indices[gl_VertexID];
	gl_Position = MVP * vec4(pos[idx], 1.0);
	// If we are rendering a wireframe pass, set the vertex color to black
	color = isWireframe > 0 ? vec3(0.0) : col[idx];
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

// declare a C++ structure to hold our uniform buffer data
struct PerFrameData
{
	// store the premultiplied model-view-projection matrix
	mat4 mvp;
	//  used to set the color of the wireframe rendering
	int isWireframe;
};

int main(void)
{
	glfwSetErrorCallback(
		[](int error, const char* description)
		{
			fprintf(stderr, "Error: %s\n", description);
		}
	);

	if (!glfwInit())
		exit(EXIT_FAILURE);

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	GLFWwindow* window = glfwCreateWindow(1024, 768, "Simple example", nullptr, nullptr);
	if (!window)
	{
		glfwTerminate();
		exit(EXIT_FAILURE);
	}

	glfwSetKeyCallback(
		window,
		[](GLFWwindow* window, int key, int scancode, int action, int mods)
		{
			if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
				glfwSetWindowShouldClose(window, GLFW_TRUE);
		}
	);

	glfwMakeContextCurrent(window);
	gladLoadGL(glfwGetProcAddress);
	glfwSwapInterval(1);

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

	GLuint vao;
	glCreateVertexArrays(1, &vao);
	glBindVertexArray(vao);

	//  use the Direct-State-Access(DSA) functions from OpenGL 4.6 to allocate the buffer object to hold the per-frame data
	const GLsizeiptr kBufferSize = sizeof(PerFrameData);
	GLuint perFrameDataBuffer;
	glCreateBuffers(1, &perFrameDataBuffer);
	// Allocates kBufferSize storage units (usually bytes) of OpenGL server memory for storing data or indices
	// Specify the storage for our uniform buffer
	glNamedBufferStorage(
		// the buffer affected
		perFrameDataBuffer,
		// number of bytes to allocate
		kBufferSize,
		// since nullptr is provided, kBufferSize units of storage are reserved for use but are left uninitialized
		nullptr,
		// tells the OpenGL implementation that the content of the data store might be updated after creation through calls to glBufferSubData().
		GL_DYNAMIC_STORAGE_BIT);
	// Associates the buffer object with the named uniform block (in the shader) associated with index 0
	// Make the entire buffer accessible from GLSL shaders at binding point 0
	glBindBufferRange(
		// the buffer binding targets
		GL_UNIFORM_BUFFER,
		// the index associated with a uniform block
		// This value should be used in the shader code to read data from the buffer
		0,
		// the buffer object
		perFrameDataBuffer,
		// the following two specify the starting index and range of the buffer that is to be mapped to the uniform buffer
		0,
		kBufferSize);

	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);

	//  a depth test is required to render a 3D cube correctly
	glEnable(GL_DEPTH_TEST);

	//  Polygon offset is needed to render a wireframe image of the cube on top of the solid image without Z-fighting
	// OpenGL Redbook: Page 171
	glEnable(GL_POLYGON_OFFSET_LINE);
	// The values of -1.0 will move the wireframe rendering slightly toward the camera
	glPolygonOffset(-1.0f, -1.0f);

	while (!glfwWindowShouldClose(window))
	{
		int width, height;
		glfwGetFramebufferSize(window, &width, &height);
		const float ratio = width / (float)height;

		glViewport(0, 0, width, height);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// To rotate the cube, the model matrix is calculated:
		const mat4 m = glm::rotate(
			glm::translate(mat4(1.0f), vec3(0.0f, 0.0f, -3.5f)),
			//  the angle of rotation is based on the current system time returned by glfwGetTime()
			(float)glfwGetTime(),
			// To rotate the cube, the model matrix is calculated as a rotation around the diagonal (1, 1, 1) axis
			vec3(1.0f, 1.0f, 1.0f));

		const mat4 p = glm::perspective(45.0f, ratio, 0.1f, 1000.0f);

		// To highlight the edges of our object, we first draw the object with polygon mode set to GL_FILL
		// and then draw it again in a different color and with the polygon mode set to GL_LINE

		//  update the buffer for the first draw call
		PerFrameData perFrameData = {.mvp = p * m, .isWireframe = false};
		// Replaces a subset of a buffer object’s data store with new data
		glNamedBufferSubData(
			// buffer to be updated
			perFrameDataBuffer,
			// The section of the buffer object specified in buffer starting at offset bytes is updated with
			// the size bytes of data addressed by data:

			// offset
			0,
			// size
			kBufferSize,
			// data
			&perFrameData);

		// render the solid cube with the polygon mode set to GL_FILL
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		// Note: gl_VertexID ranges from [0, 36), which is specified here
		glDrawArrays(GL_TRIANGLES, 0, 36);

		// update the buffer for the second draw call
		perFrameData.isWireframe = true;
		glNamedBufferSubData(perFrameDataBuffer, 0, kBufferSize, &perFrameData);

		//  render the wireframe cube using the GL_LINE polygon mode
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		glDrawArrays(GL_TRIANGLES, 0, 36);

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	glDeleteBuffers(1, &perFrameDataBuffer);
	glDeleteProgram(program);
	glDeleteShader(shaderFragment);
	glDeleteShader(shaderVertex);
	glDeleteVertexArrays(1, &vao);

	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}
