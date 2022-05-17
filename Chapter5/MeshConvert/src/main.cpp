// include mandatory header files
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/cimport.h>
#include "shared/scene/VtxData.h"

#include <meshoptimizer.h>

// This mesh conversion tool preprocess a mesh so that we can store it in a runtime efficient data format


// TODO: There is no global verbose flag as mentioned at page 246

MeshData g_meshData;

// To fill MeshData's index/vertex fields,
// we require two counters to track offsets of index and vertex mesh data inside the file
uint32_t g_indexOffset = 0;
uint32_t g_vertexOffset = 0;

// TODO: There are no flags controlling whether we need to export texture coordinates and normal vectors as mentioned at page 246

// By default, we export vertex coordinates (3), normal (3), and texture coordinates (2).
// Therefore, 3+3+2 elements are specified here 
constexpr uint32_t g_numElementsToStore = 3 + 3 + 2;

// TODO: Make it so that we can parse command-line arguments to check whether texture coordinates or normal vectors are needed

// By default, the mesh scale is 0.01
float g_meshScale = 0.01f;

// By default, we don't calculate LODs
bool g_calculateLODs = false;

/**
 * \brief Create LOD indices
 * \param indices The original indices
 * \param vertices The original vertices
 * \param outLods The output collection of indices that represent LOD meshes
 */
void processLods(std::vector<uint32_t>& indices,
                 std::vector<float>& vertices,
                 std::vector<std::vector<uint32_t>>& outLods)
{
	// Each vertex is constructed from 3 float values
	// TODO: in the book, size_t verticesCountIn = vertices.size() / 3;
	size_t verticesCountIn = vertices.size() / 2;
	size_t targetIndicesCount = indices.size();

	uint8_t LOD = 1;

	printf("\n   LOD0: %i indices", int(indices.size()));

	outLods.push_back(indices);

	while (targetIndicesCount > 1024 && LOD < 8)
	{
		targetIndicesCount = indices.size() / 2;

		bool sloppy = false;

		size_t numOptIndices = meshopt_simplify(indices.data(),
		                                        indices.data(),
		                                        (uint32_t)indices.size(),
		                                        vertices.data(),
		                                        verticesCountIn,
		                                        sizeof(float) * 3,
		                                        targetIndicesCount,
		                                        0.02f);

		// cannot simplify further
		if (static_cast<size_t>(numOptIndices * 1.1f) > indices.size())
		{
			if (LOD > 1)
			{
				// try harder
				numOptIndices = meshopt_simplifySloppy(
					indices.data(),
					indices.data(), indices.size(),
					vertices.data(), verticesCountIn,
					sizeof(float) * 3,
					targetIndicesCount, 0.02f);
				sloppy = true;
				if (numOptIndices == indices.size()) break;
			}
			else
				break;
		}

		indices.resize(numOptIndices);

		meshopt_optimizeVertexCache(indices.data(), indices.data(), indices.size(), verticesCountIn);

		printf("\n   LOD%i: %i indices %s", int(LOD), int(numOptIndices), sloppy ? "[sloppy]" : "");

		LOD++;

		outLods.push_back(indices);
	}
}

/**
 * \brief This function converts an input assimp mesh into a mesh of our own representation
 * \param m The input Assimp mesh
 * \return The mesh of our own representation
 */
Mesh convertAIMesh(const aiMesh* m)
{
	// check whether a set of texture coordinates is present in the original Assimp mesh
	const bool hasTexCoords = m->HasTextureCoords(0);
	// The size of the stream element in bytes is directly calculated from the number of elements per vertex
	const uint32_t streamElementSize = static_cast<uint32_t>(g_numElementsToStore * sizeof(float));

	// Original data for LOD calculation
	std::vector<float> srcVertices;
	std::vector<uint32_t> srcIndices;

	// if we don't have LOD, the first element in this vector is the original data indices
	std::vector<std::vector<uint32_t>> outLods;

	auto& vertices = g_meshData.vertexData_;

	// For each of the vertices, we extract their data from the aiMesh object
	for (size_t i = 0; i != m->mNumVertices; i++)
	{
		// we export vertex, normal, and texture coordinate
		const aiVector3D v = m->mVertices[i];
		const aiVector3D n = m->mNormals[i];
		const aiVector3D t = hasTexCoords ? m->mTextureCoords[0][i] : aiVector3D();

		if (g_calculateLODs)
		{
			srcVertices.push_back(v.x);
			srcVertices.push_back(v.y);
			srcVertices.push_back(v.z);
		}

		// append vertex, texture coordinate, and normal to the vertex stream
		vertices.push_back(v.x * g_meshScale);
		vertices.push_back(v.y * g_meshScale);
		vertices.push_back(v.z * g_meshScale);

		vertices.push_back(t.x);
		vertices.push_back(1.0f - t.y); // note: y-coordinate is flipped

		vertices.push_back(n.x);
		vertices.push_back(n.y);
		vertices.push_back(n.z);
	}

	Mesh result = {
		// we only have 1 vertex stream per mesh
		.streamCount = 1,
		// set the index and vertex offset within the input file
		.indexOffset = g_indexOffset,
		.vertexOffset = g_vertexOffset,
		// vertex count is the number of vertices in this mesh
		.vertexCount = m->mNumVertices,
		// update stream offset and element size for this mesh
		.streamOffset = {g_vertexOffset * streamElementSize},
		.streamElementSize = {streamElementSize}
	};

	for (size_t i = 0; i != m->mNumFaces; i++)
	{
		// skip non-triangle faces
		if (m->mFaces[i].mNumIndices != 3)
			continue;
		// populate data indices
		for (unsigned j = 0; j != m->mFaces[i].mNumIndices; j++)
		{
			srcIndices.push_back(m->mFaces[i].mIndices[j]);
		}
	}

	if (!g_calculateLODs)
	{
		outLods.push_back(srcIndices);
	}
	else
	{
		processLods(srcIndices, srcVertices, outLods);
	}

	printf("\nCalculated LOD count: %u\n", (unsigned)outLods.size());

	// put LOD indices into Mesh's indices
	uint32_t numIndices = 0;
	for (size_t l = 0; l < outLods.size(); l++)
	{
		for (size_t i = 0; i < outLods[l].size(); i++)
		{
			g_meshData.indexData_.push_back(outLods[l][i]);
		}

		result.lodOffset[l] = numIndices;
		numIndices += (int)outLods[l].size();
	}

	// last item of loadOffset array is used for special purpose
	result.lodOffset[outLods.size()] = numIndices;

	result.lodCount = (uint32_t)outLods.size();

	// After processing the input mesh, we increment offset counters for the indices and current starting vertex
	g_indexOffset += numIndices;
	g_vertexOffset += m->mNumVertices;

	return result;
}

/**
 * \brief This function processes the input file.
 * This includes loading the scene and converting each mesh into an internal format (our own mesh representation)
 * \param fileName The input file name
 */
void loadFile(const char* fileName)
{
	printf("Loading '%s'...\n", fileName);

	// The list of flags for the ASSIMP import function
	// TODO: aiProcess_PreTransformVertices, aiProcess_FindInstances, aiProcess_OptimizeMeshes are provided in the book, but are here
	// TODO: aiProcess_LimitBoneWeights, aiProcess_SplitLargeMeshes, aiProcess_ImproveCacheLocality are provided here, but not in the book
	const unsigned int flags = 0 |
		aiProcess_JoinIdenticalVertices |
		aiProcess_Triangulate |
		aiProcess_GenSmoothNormals |
		aiProcess_LimitBoneWeights |
		aiProcess_SplitLargeMeshes |
		aiProcess_ImproveCacheLocality |
		aiProcess_RemoveRedundantMaterials |
		aiProcess_FindDegenerates |
		aiProcess_FindInvalidData |
		aiProcess_GenUVCoords;

	const aiScene* scene = aiImportFile(fileName, flags);

	if (!scene || !scene->HasMeshes())
	{
		printf("Unable to load '%s'\n", fileName);
		exit(255);
	}

	// After importing the scene, we reserve the memory for the mesh descriptor and bounding box containers accordingly 
	g_meshData.meshes_.reserve(scene->mNumMeshes);
	g_meshData.boxes_.reserve(scene->mNumMeshes);

	// call convertAIMesh for each mesh in the scene
	for (unsigned int i = 0; i != scene->mNumMeshes; i++)
	{
		printf("\nConverting meshes %u/%u...", i + 1, scene->mNumMeshes);
		fflush(stdout);
		g_meshData.meshes_.push_back(convertAIMesh(scene->mMeshes[i]));
	}

	// TODO: ignore this in this chapter for now
	recalculateBoundingBoxes(g_meshData);
}

int main()
{
	loadFile("deps/src/bistro/Exterior/exterior.obj");

	std::vector<DrawData> grid;
	g_vertexOffset = 0;
	for (auto i = 0; i < g_meshData.meshes_.size(); i++)
	{
		grid.push_back(DrawData{
			.meshIndex = (uint32_t)i,
			.materialIndex = 0,
			.LOD = 0,
			.indexOffset = g_meshData.meshes_[i].indexOffset,
			.vertexOffset = g_vertexOffset,
			.transformIndex = 0
		});
		g_vertexOffset += g_meshData.meshes_[i].vertexCount;
	}

	saveMeshData("data/meshes/test.meshes", g_meshData);

	FILE* f = fopen("data/meshes/test.meshes.drawdata", "wb");
	fwrite(grid.data(), grid.size(), sizeof(DrawData), f);
	fclose(f);

	return 0;
}
