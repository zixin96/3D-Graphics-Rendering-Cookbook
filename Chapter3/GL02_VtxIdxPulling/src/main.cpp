#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/cimport.h>
#include <assimp/version.h>

#include <stdio.h>
#include <stdlib.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include "shared/glFramework/GLShader.h"
#include "shared/debug.h"

#include <vector>

// Let's render the same duck, but using PVP pipelines (vertex pulling technique)
// The idea is to allocate two buffer objects – one for the indices and another for the vertex data
// and access them in GLSL shaders as shader storage buffers

using glm::mat4;
using glm::vec3;
using glm::vec2;

struct PerFrameData
{
	mat4 mvp;
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
	glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);

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

	initDebug();

	GLShader shaderVertex("data/shaders/chapter03/GL02Index.vert");
	GLShader shaderGeometry("data/shaders/chapter03/GL02.geom");
	GLShader shaderFragment("data/shaders/chapter03/GL02.frag");
	GLProgram program(shaderVertex, shaderGeometry, shaderFragment);
	program.useProgram();

	const GLsizeiptr kUniformBufferSize = sizeof(PerFrameData);

	GLuint perFrameDataBuffer;
	glCreateBuffers(1, &perFrameDataBuffer);
	glNamedBufferStorage(perFrameDataBuffer, kUniformBufferSize, nullptr, GL_DYNAMIC_STORAGE_BIT);
	glBindBufferRange(GL_UNIFORM_BUFFER, 0, perFrameDataBuffer, 0, kUniformBufferSize);

	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
	glEnable(GL_DEPTH_TEST);

	const aiScene* scene = aiImportFile("data/rubber_duck/scene.gltf", aiProcess_Triangulate);

	if (!scene || !scene->HasMeshes())
	{
		printf("Unable to load data/rubber_duck/scene.gltf\n");
		exit(255);
	}

	// Next, convert the per-vertex data into a format suitable for our GLSL shaders

	struct VertexData
	{
		vec3 pos;
		vec2 uv;
	};

	const aiMesh* mesh = scene->mMeshes[0];
	std::vector<VertexData> vertices;
	for (unsigned i = 0; i != mesh->mNumVertices; i++)
	{
		const aiVector3D v = mesh->mVertices[i];
		const aiVector3D t = mesh->mTextureCoords[0][i];
		vertices.push_back({.pos = vec3(v.x, v.z, v.y), .uv = vec2(t.x, t.y)});
	}

	// TODO: ensure we can switch between 32 and 16 bit indices
	std::vector<unsigned int> indices;
	for (unsigned i = 0; i != mesh->mNumFaces; i++)
	{
		for (unsigned j = 0; j != 3; j++)
			indices.push_back(mesh->mFaces[i].mIndices[j]);
	}
	aiReleaseImport(scene);

	const size_t kSizeIndices = sizeof(unsigned int) * indices.size();
	const size_t kSizeVertices = sizeof(VertexData) * vertices.size();

	// indices
	GLuint dataIndices;
	glCreateBuffers(1, &dataIndices);
	glNamedBufferStorage(dataIndices, kSizeIndices, indices.data(), 0);
	glBindBufferBase(
		GL_SHADER_STORAGE_BUFFER,
		// bind our vertex data shader storage buffer to binding point 2
		2,
		dataIndices);

	// vertices
	GLuint dataVertices;
	glCreateBuffers(1, &dataVertices);
	glNamedBufferStorage(dataVertices, kSizeVertices, vertices.data(), 0);
	glBindBufferBase(
		GL_SHADER_STORAGE_BUFFER,
		// bind our vertex data shader storage buffer to binding point 1
		1, 
		dataVertices);

	GLuint vao;
	glCreateVertexArrays(1, &vao);
	glBindVertexArray(vao);

	// texture
	int w, h, comp;
	const uint8_t* img = stbi_load("data/rubber_duck/textures/Duck_baseColor.png", &w, &h, &comp, 3);

	GLuint texture;
	glCreateTextures(GL_TEXTURE_2D, 1, &texture);
	glTextureParameteri(texture, GL_TEXTURE_MAX_LEVEL, 0);
	glTextureParameteri(texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTextureParameteri(texture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTextureStorage2D(texture, 1, GL_RGB8, w, h);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTextureSubImage2D(texture, 0, 0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, img);
	glBindTextures(0, 1, &texture);

	stbi_image_free((void*)img);

	while (!glfwWindowShouldClose(window))
	{
		int width, height;
		glfwGetFramebufferSize(window, &width, &height);
		const float ratio = width / (float)height;

		glViewport(0, 0, width, height);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		const mat4 m = glm::rotate(glm::translate(mat4(1.0f), vec3(0.0f, -0.5f, -1.5f)), (float)glfwGetTime(),
		                           vec3(0.0f, 1.0f, 0.0f));
		const mat4 p = glm::perspective(45.0f, ratio, 0.1f, 1000.0f);

		const PerFrameData perFrameData = {.mvp = p * m};
		glNamedBufferSubData(perFrameDataBuffer, 0, kUniformBufferSize, &perFrameData);
		// Non-indexed draw is used, and gl_VertexID is used to manually read the VertexID from the index buffer
		glDrawArrays(GL_TRIANGLES, 0, static_cast<unsigned>(indices.size()));
		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	glDeleteBuffers(1, &dataIndices);
	glDeleteBuffers(1, &dataVertices);
	glDeleteBuffers(1, &perFrameDataBuffer);
	glDeleteVertexArrays(1, &vao);

	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}