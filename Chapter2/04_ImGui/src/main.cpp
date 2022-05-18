#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <imgui/imgui.h>
#include <glm/glm.hpp>
#include <glm/ext.hpp>

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

using glm::mat4;

// Let's do ImGui UI rendering

int main()
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

	GLFWwindow* window = glfwCreateWindow(1280, 720, "Simple example", nullptr, nullptr);
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

	glfwSetCursorPosCallback(
		window,
		[](auto* window, double x, double y)
		{
			// To enable mouse cursor interaction, we need to pass cursor info from GLFW to ImGui
			ImGui::GetIO().MousePos = ImVec2((float)x, (float)y);
		}
	);

	glfwSetMouseButtonCallback(
		window,
		[](auto* window, int button, int action, int mods)
		{
			// To enable mouse button interaction, we need to pass mouse button info from GLFW to ImGui
			auto& io = ImGui::GetIO();
			const int idx = button == GLFW_MOUSE_BUTTON_LEFT ? 0 : button == GLFW_MOUSE_BUTTON_RIGHT ? 2 : 1;
			io.MouseDown[idx] = action == GLFW_PRESS;
		}
	);

	glfwMakeContextCurrent(window);
	gladLoadGL(glfwGetProcAddress);
	glfwSwapInterval(1);

	// To render geometry data coming from ImGui, we need a VAO with vertex and index buffers
	GLuint vao;
	glCreateVertexArrays(1, &vao);

	GLuint handleVBO;
	glCreateBuffers(1, &handleVBO);
	// use an upper limit of 128KB for vertices data
	glNamedBufferStorage(handleVBO, 128 * 1024, nullptr, GL_DYNAMIC_STORAGE_BIT);

	GLuint handleElements;
	glCreateBuffers(1, &handleElements);
	// use an upper limit of 256KB for indices data
	glNamedBufferStorage(handleElements, 256 * 1024, nullptr, GL_DYNAMIC_STORAGE_BIT);

	// Bind a buffer containing indices to this VAO
	glVertexArrayElementBuffer(vao, handleElements);

	// Bind a buffer containing the interleaved vertex data to this VAO's buffer binding point 0
	glVertexArrayVertexBuffer(
		// the name of the vertex array object
		vao,
		// buffer binding point
		0,
		// The name of a buffer to bind to the vertex buffer binding point
		handleVBO,
		// The offset of the first element of the buffer
		0,
		// The distance between elements within the buffer
		sizeof(ImDrawVert));

	// Next, configure the vertex attributes, which contains 2D vertex positions, texture coordinates, and RGBA colors (ImDrawVert, part of IMGUI)

	// Enable all three vertex attributes streams
	glEnableVertexArrayAttrib(
		vao,
		// the index of the generic vertex attribute stream to be enabled
		0);
	glEnableVertexArrayAttrib(vao, 1);
	glEnableVertexArrayAttrib(vao, 2);

	// Specify a data format for each attribute stream
	glVertexArrayAttribFormat(
		vao,
		// attribute stream index (correspond to the location binding points in the GLSL shaders)
		0,
		// The number of values per vertex that are stored in the array
		2,
		// The type of the data stored in the array
		GL_FLOAT,
		// normalized
		GL_FALSE,
		// offset
		IM_OFFSETOF(ImDrawVert, pos));
	glVertexArrayAttribFormat(vao, 1, 2, GL_FLOAT, GL_FALSE, IM_OFFSETOF(ImDrawVert, uv));
	glVertexArrayAttribFormat(
		vao,
		2,
		4,
		GL_UNSIGNED_BYTE,
		// true since the parameter represents a normalized integer
		GL_TRUE,
		IM_OFFSETOF(ImDrawVert, col));

	// tell OpenGL to read the data for streams 0, 1, and 2 from the buffer, which is sattached to buffer binding point 0
	glVertexArrayAttribBinding(
		vao,
		// attribute stream number 
		0,
		// buffer bind point (specified in glVertexArrayVertexBuffer)
		0);
	glVertexArrayAttribBinding(vao, 1, 0);
	glVertexArrayAttribBinding(vao, 2, 0);

	glBindVertexArray(vao);

	// Now, let's take a quick look at the shaders that are used to render our UI:
	const GLchar* shaderCodeVertex = R"(
		#version 460 core
		layout (location = 0) in vec2 Position;
		layout (location = 1) in vec2 UV;
		layout (location = 2) in vec4 Color;
		layout(std140, binding = 0) uniform PerFrameData
		{
			uniform mat4 MVP;
		};
		out vec2 Frag_UV;
		out vec4 Frag_Color;
		void main()
		{
			Frag_UV = UV;
			Frag_Color = Color;
			gl_Position = MVP * vec4(Position.xy,0,1);
		}
	)";

	const GLchar* shaderCodeFragment = R"(
		#version 460 core
		in vec2 Frag_UV;
		in vec4 Frag_Color;
		layout (binding = 0) uniform sampler2D Texture;
		layout (location = 0) out vec4 Out_Color;
		void main()
		{
			// modulates the vertex color with a texture
			Out_Color = Frag_Color * texture(Texture, Frag_UV.st);
		}
	)";

	const GLuint handleVertex = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(handleVertex, 1, &shaderCodeVertex, nullptr);
	glCompileShader(handleVertex);

	const GLuint handleFragment = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(handleFragment, 1, &shaderCodeFragment, nullptr);
	glCompileShader(handleFragment);

	const GLuint program = glCreateProgram();
	glAttachShader(program, handleVertex);
	glAttachShader(program, handleFragment);
	glLinkProgram(program);
	glUseProgram(program);

	GLuint perFrameDataBuffer;
	glCreateBuffers(1, &perFrameDataBuffer);
	glNamedBufferStorage(perFrameDataBuffer, sizeof(mat4), nullptr, GL_DYNAMIC_STORAGE_BIT);
	// Calling glBindBufferBase() is identical to calling glBindBufferRange() with offset equal to zero and size equal to the size of the buffer object
	glBindBufferBase(
		// the target of the bind operation
		GL_UNIFORM_BUFFER,
		// Specify the index of the binding point
		0,
		// The name of a buffer object to bind to the specified binding point
		perFrameDataBuffer);

	// There are some initialization steps that need to be done for ImGui itself

	// set up the data structures that are needed to sustain an ImGui context
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	// tell ImGui we are using glDrawElementsBaseVertex for rendering, which has base vertex offset
	io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;

	// Build texture atlas for font rendering
	ImFontConfig cfg = ImFontConfig();
	// Tell ImGui that we are going to manage the memory ourselves
	cfg.FontDataOwnedByAtlas = false;
	// Brighten up the font a little bit to make them more readable
	cfg.RasterizerMultiply = 1.5f;
	// Calculate the pixel height of the font. We take our default window height of 768
	// and divide it by the desired number of text lines to be fit in the window
	cfg.SizePixels = 768.0f / 32.0f;

	// improve the appearance of the text on the screen by aligning every glyph to the pixel boundary
	// and rasterize them at a higher quality for sub-pixel positioning
	cfg.PixelSnapH = true;
	cfg.OversampleH = 4;
	cfg.OversampleV = 4;

	// load a .ttf font from a file
	ImFont* Font = io.Fonts->AddFontFromFileTTF("data/OpenSans-Light.ttf", cfg.SizePixels, &cfg);

	// extract the font atlas bitmap data from ImGui n 32-bit RGBA format
	unsigned char* pixels = nullptr;
	int width, height;
	io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

	// upload the font atlas bitmap data to OpenGL
	GLuint texture;
	glCreateTextures(GL_TEXTURE_2D, 1, &texture);
	glTextureParameteri(texture, GL_TEXTURE_MAX_LEVEL, 0);
	glTextureParameteri(texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTextureParameteri(texture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTextureStorage2D(texture, 1, GL_RGBA8, width, height);
	// Scanlines in the ImGui bitmap are not padded. Disable the pixel unpack alignment in OpenGL
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTextureSubImage2D(texture, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
	glBindTextures(0, 1, &texture);

	// pass the texture handle to ImGui so that we can use it in subsequent draw calls when required
	io.Fonts->TexID = (ImTextureID)(intptr_t)texture;
	io.FontDefault = Font;
	io.DisplayFramebufferScale = ImVec2(1, 1);

	// OpenGL state setup for rendering:
	// blending and the scissor test should be turned on
	glEnable(GL_BLEND);
	glEnable(GL_SCISSOR_TEST);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	// the depth test and backface culling should disabled
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);

	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);

	while (!glfwWindowShouldClose(window))
	{
		int width, height;
		glfwGetFramebufferSize(window, &width, &height);

		glViewport(0, 0, width, height);
		glClear(GL_COLOR_BUFFER_BIT);

		ImGuiIO& io = ImGui::GetIO();
		// Tell ImGui our current window dimensions
		io.DisplaySize = ImVec2((float)width, (float)height);
		// start a new frame
		ImGui::NewFrame();
		// render a demo UI window
		ImGui::ShowDemoWindow();
		// The geometry data is generated in the ImGui::Render() function
		ImGui::Render();
		// and can be retrieved via ImGui::GetDrawData()
		const ImDrawData* draw_data = ImGui::GetDrawData();

		// construct and upload a proper orthographic projection matrix based on values provided by Imgui
		const float L = draw_data->DisplayPos.x;
		const float R = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
		const float T = draw_data->DisplayPos.y;
		const float B = draw_data->DisplayPos.y + draw_data->DisplaySize.y;
		const mat4 orthoProjection = glm::ortho(L, R, B, T);
		glNamedBufferSubData(perFrameDataBuffer, 0, sizeof(mat4), glm::value_ptr(orthoProjection));

		// go through all of the ImGui command lists, update the content of
		// the index and vertex buffers, and invoke the rendering commands
		for (int n = 0; n < draw_data->CmdListsCount; n++)
		{
			// Each ImGui command list has vertex and index data associated with it. Use this data
			// to update the appropriate OpenGL buffers
			const ImDrawList* cmd_list = draw_data->CmdLists[n];
			glNamedBufferSubData(handleVBO, 0, (GLsizeiptr)cmd_list->VtxBuffer.Size * sizeof(ImDrawVert),
				cmd_list->VtxBuffer.Data);
			glNamedBufferSubData(handleElements, 0, (GLsizeiptr)cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx),
				cmd_list->IdxBuffer.Data);

			// Rendering commands are stored inside the command buffer. Iterate over them and
			// render the actual geometry
			for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
			{
				const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
				const ImVec4 cr = pcmd->ClipRect;
				glScissor((int)cr.x, (int)(height - cr.w), (int)(cr.z - cr.x), (int)(cr.w - cr.y));
				glBindTextureUnit(0, (GLuint)(intptr_t)pcmd->TextureId);
				// render primitives from array data with a per-element offset
				glDrawElementsBaseVertex(
					// what kind of primitives to render
					GL_TRIANGLES,
					// Specifies the number of elements to be rendered
					(GLsizei)pcmd->ElemCount,
					// the type of the values in indices
					GL_UNSIGNED_SHORT,
					// Specifies a pointer to the location where the indices are stored
					(void*)(intptr_t)(pcmd->IdxOffset * sizeof(ImDrawIdx)),
					// Specifies a constant that should be added to each element of indices when choosing elements from the enabled vertex arrays
					(GLint)pcmd->VtxOffset);
			}
		}

		// reset the scissor rectangle after UI rendering is complete
		glScissor(0, 0, width, height);

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	// destroy the ImGui context 
	ImGui::DestroyContext();

	// OpenGL object deletion (omitted for brevity)

	glfwDestroyWindow(window);

	glfwTerminate();
	exit(EXIT_SUCCESS);
}