#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <filesystem>

// Notice that the STB library has separate headers for loading and saving images
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

#include <stdio.h>
#include <stdlib.h>

using glm::mat4;
using glm::vec3;

static const char* shaderCodeVertex = R"(
#version 460 core
layout(std140, binding = 0) uniform PerFrameData
{
	uniform mat4 MVP;
};
layout (location=0) out vec2 uv;
const vec2 pos[3] = vec2[3](
	vec2(-0.6f, -0.4f),
	vec2( 0.6f, -0.4f),
	vec2( 0.0f,  0.6f)
);
const vec2 tc[3] = vec2[3](
	vec2( 0.0, 0.0 ),
	vec2( 1.0, 0.0 ),
	vec2( 0.5, 1.0 )
);
void main()
{
	gl_Position = MVP * vec4(pos[gl_VertexID], 0.0, 1.0);
	uv = tc[gl_VertexID];
}
)";

static const char* shaderCodeFragment = R"(
#version 460 core
layout (location=0) in vec2 uv;
layout (location=0) out vec4 out_FragColor;
layout (binding=0) uniform sampler2D texture0;
void main()
{
	out_FragColor = texture(texture0, uv);
};
)";

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
			// we can save images into various image file formats by using the STB library
			// save a screenshot from an OpenGL GLFW application
			if (key == GLFW_KEY_F9 && action == GLFW_PRESS)
			{
				int width, height;
				glfwGetFramebufferSize(window, &width, &height);
				uint8_t* ptr = (uint8_t*)malloc(width * height * 4);
				// Reads pixel data from the read framebuffer rectangle whose lower-left
				// corner is at(x, y) in window coordinates and whose dimensions are width
				// and height, and then stores the data in the array pointed to by pixels.
				glReadPixels(
					// x
					0,
					// y
					0,
					// width
					width,
					// height
					height,
					// format: the kind of pixel data elements that are read (depth, color, or stencil)
					GL_RGBA,
					// type: the data type of each element
					GL_UNSIGNED_BYTE,
					// pixels
					ptr);
				// TODO: use "outputs/screenshot.png" instead of "screenshot.png" for better organization
				stbi_write_png("outputs/screenshot.png", width, height, 4, ptr, 0);
				free(ptr);
			}
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

	GLuint vao;
	glCreateVertexArrays(1, &vao);
	glBindVertexArray(vao);

	const GLsizeiptr kBufferSize = sizeof(mat4);

	GLuint perFrameDataBuffer;
	glCreateBuffers(1, &perFrameDataBuffer);
	glNamedBufferStorage(perFrameDataBuffer, kBufferSize, nullptr, GL_DYNAMIC_STORAGE_BIT);
	glBindBufferRange(GL_UNIFORM_BUFFER, 0, perFrameDataBuffer, 0, kBufferSize);

	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);

	// load an image as a 3-channel RGB image from a jpg file
	// TODO: See Page 37: STB supports the loading of high-dynamic-range images in Radiance .HDR file format. Use the stbi_loadf() function to load files as floating-point images
	int w, h, comp;
	const uint8_t* img = stbi_load("data/ch2_sample3_STB.jpg", &w, &h, &comp, 3);

	// The loaded img image can be used as an OpenGL texture in a DSA fashion:
	GLuint texture;
	glCreateTextures(GL_TEXTURE_2D, 1, &texture);
	glTextureParameteri(texture, GL_TEXTURE_MAX_LEVEL, 0);
	glTextureParameteri(texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTextureParameteri(texture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	// allocate storage for 2D textures
	glTextureStorage2D(
		// the name of the texture object for which storage is to be allocated
		texture,
		//  the number of mipmap levels to be allocated for the texture.
		1,
		//  the internal format to be used for as storage for the texture
		GL_RGB8,
		// width and height of the texture
		w,
		h);
	// Texture data layout: red book page 275 
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	// Replace a region of the texture specified in texture with new data specified in data.
	glTextureSubImage2D(
		// texture
		texture,
		// level
		0,
		// x, y offset
		0, 0,
		// width and height
		w, h,
		// format
		GL_RGB,
		// type
		GL_UNSIGNED_BYTE,
		// data
		img);
	// bind one or more named textures to a sequence of consecutive texture units
	glBindTextures(
		// the first texture unit to which a texture is to be bound (corresponds to layout(binding=0) in fragment shader)
		0,
		// the number of textures to bind
		1,
		// the address of an array of names of existing texture objects
		&texture);

	stbi_image_free((void*)img);

	while (!glfwWindowShouldClose(window))
	{
		int width, height;
		glfwGetFramebufferSize(window, &width, &height);
		const float ratio = width / (float)height;

		glViewport(0, 0, width, height);
		glClear(GL_COLOR_BUFFER_BIT);

		const mat4 m = glm::rotate(mat4(1.0f), (float)glfwGetTime(), vec3(0.0f, 0.0f, 1.0f));
		const mat4 p = glm::ortho(-ratio, ratio, -1.f, 1.f, 1.f, -1.f);
		const mat4 mvp = p * m;

		glUseProgram(program);
		glNamedBufferSubData(perFrameDataBuffer, 0, kBufferSize, glm::value_ptr(mvp));
		glDrawArrays(GL_TRIANGLES, 0, 3);

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	glDeleteTextures(1, &texture);
	glDeleteBuffers(1, &perFrameDataBuffer);
	glDeleteProgram(program);
	glDeleteShader(shaderFragment);
	glDeleteShader(shaderVertex);
	glDeleteVertexArrays(1, &vao);

	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}
