//
#version 460 core

// This vertex shader stores indices insdie a shader storage buffer, instead of attaching EBO to VAO 

layout(std140, binding = 0) uniform PerFrameData
{
	uniform mat4 MVP;
};

// The VertexData structure here should mauvh the VertexData structure in C++ that we
// used previously to fill in the data for our buffers
struct VertexData
{
	// we use float to bypass alignment requirements
	float pos[3];
	float uv[2];
};

// declare a vertex storage buffer attached to binding point 1 and is declared as readonly
layout(std430, binding = 1) restrict readonly buffer Vertices
{
	// the buffer holds an unbounded array of VertexData[] elements
	// Each element corresponds to exactly one vertex
	VertexData in_Vertices[];
};

layout(std430, binding = 2) restrict readonly buffer Indices
{
	uint in_Indices[];
};

// the following 2 accessor functions are required to extract the vec3 position data and the vec2 texture
// coordinates data from the buffer

vec3 getPosition(uint i)
{
	// Three consecutive floats are positions
	return vec3(in_Vertices[i].pos[0], in_Vertices[i].pos[1], in_Vertices[i].pos[2]);
}

vec2 getTexCoord(uint i)
{
	// two are texture coordinates
	return vec2(in_Vertices[i].uv[0], in_Vertices[i].uv[1]);
}

layout (location=0) out vec2 uv;

void main()
{
	// fetch index from storage buffer
	uint positionIndex = in_Indices[gl_VertexID];

	vec3 pos = getPosition(positionIndex);
	gl_Position = MVP * vec4(pos, 1.0);

	uv = getTexCoord(positionIndex);
}
