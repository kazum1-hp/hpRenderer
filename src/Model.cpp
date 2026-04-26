#include "../include/Model.h"
#include <iostream>
#include <filesystem>

Model::Model(const std::string& path)
{
	loadModel(path);
}

void Model::draw() const
{
    if (instancingEnabled)
    {
        for (const auto& mesh : meshes)
            mesh -> drawInstanced(instanceCount);
    }
    else
    {
        for (const auto& mesh : meshes)
            mesh -> draw();
    }
}

void Model::enableInstancing(const std::vector<glm::mat4>& instanceTransforms)
{
    instanceCount = static_cast<unsigned int>(instanceTransforms.size());
    instancingEnabled = true;

    for (auto& mesh : meshes)
    {
        mesh -> setInstanceTransforms(instanceTransforms);
    }
}

// New: Safely reload the model on the current instance
void Model::reload(const std::string& path)
{
    // Clean up previous data (unique_ptr will delete GPU resources during destruction).
    meshes.clear();
    loadedTextures.clear();
    directory.clear();

    // Reload
    loadModel(path);
}

void Model::loadModel(const std::string& path)
{
	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(path,
		aiProcess_Triangulate |
		aiProcess_FlipUVs |
		aiProcess_CalcTangentSpace);

	if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
		std::cerr << "ERROR::ASSIMP:: " << importer.GetErrorString() << std::endl;
		return;
	}

	// Use std::filesystem to get the parent directory, compatible with different path styles.
	std::filesystem::path p(path);
	directory = p.parent_path().string();

	for (unsigned int i = 0; i < scene->mNumMeshes; i++)
	{
		meshes.push_back(processMesh(scene->mMeshes[i], scene));
	}
}

std::unique_ptr<Mesh> Model::processMesh(aiMesh* mesh, const aiScene* scene)
{
	std::vector<float> vertices;
	std::vector<unsigned int> indices;
	std::vector<VertexAttribute> attributes;

	for (unsigned int i = 0; i < mesh->mNumVertices; i++)
	{
		vertices.push_back(mesh->mVertices[i].x);
		vertices.push_back(mesh->mVertices[i].y);
		vertices.push_back(mesh->mVertices[i].z);
			// Normal
		if (mesh->HasNormals()) {
			vertices.push_back(mesh->mNormals[i].x);
			vertices.push_back(mesh->mNormals[i].y);
			vertices.push_back(mesh->mNormals[i].z);
		}
		else {
			vertices.push_back(0.0f);
			vertices.push_back(0.0f);
			vertices.push_back(0.0f);
		}
			// TexCoord
		if (mesh->mTextureCoords[0]) 
		{
			vertices.push_back(mesh->mTextureCoords[0][i].x);
			vertices.push_back(mesh->mTextureCoords[0][i].y);
		}

		else {
			vertices.push_back(0.0f);
			vertices.push_back(0.0f);
		}
			// Tangent
		if (mesh->HasTangentsAndBitangents()) {
			vertices.push_back(mesh->mTangents[i].x);
			vertices.push_back(mesh->mTangents[i].y);
			vertices.push_back(mesh->mTangents[i].z);

			// --- Bitangent ---
			vertices.push_back(mesh->mBitangents[i].x);
			vertices.push_back(mesh->mBitangents[i].y);
			vertices.push_back(mesh->mBitangents[i].z);
		}
		else {
			// default
			vertices.push_back(0.0f);
			vertices.push_back(0.0f);
			vertices.push_back(0.0f);
			vertices.push_back(0.0f);
			vertices.push_back(0.0f);
			vertices.push_back(0.0f);
		}
	}

	for (unsigned int i = 0; i < mesh->mNumFaces; i++)
	{
		aiFace face = mesh->mFaces[i];
		for (unsigned int j = 0; j < face.mNumIndices; j++)
			indices.push_back(face.mIndices[j]);
	}

	GLuint index = 0;
	attributes.push_back({ index++, 3, GL_FLOAT, GL_FALSE }); // pos	
	attributes.push_back({ index++, 3, GL_FLOAT, GL_FALSE }); // normal
	attributes.push_back({ index++, 2, GL_FLOAT, GL_FALSE }); // uv
	attributes.push_back({ index++, 3, GL_FLOAT, GL_FALSE }); // tangent
	attributes.push_back({ index++, 3, GL_FLOAT, GL_FALSE }); // bitangent

	Geometry geometry(vertices, indices, attributes);

	aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
	std::vector<std::shared_ptr<Texture>> meshTextures; 
	for (int type = aiTextureType_DIFFUSE; type <= AI_TEXTURE_TYPE_MAX; ++type) {
		auto textures = loadMaterialTextures(material, static_cast<aiTextureType>(type), std::to_string(type));
		meshTextures.insert(meshTextures.end(), textures.begin(), textures.end());
	}

	return std::make_unique<Mesh>(geometry, meshTextures);
}

std::vector<std::shared_ptr<Texture>> Model::loadMaterialTextures(aiMaterial* mat, aiTextureType type, const std::string& typeName)
{
	std::vector<std::shared_ptr<Texture>> textures;

	for (unsigned int i = 0; i < mat->GetTextureCount(type); i++) 
	{
		aiString str;
		mat->GetTexture(type, i, &str);

		// Use filesystem concatenation to avoid errors from manual concatenation.
		std::filesystem::path texpath = std::filesystem::path(directory) / std::filesystem::path(str.C_Str());
		std::string path = texpath.string();

		std::cout << "[Assimp] Found texture: " << path << " | Type: " << typeName << std::endl;

		if (loadedTextures.find(path) == loadedTextures.end()) 
		{
			TextureType texType;

			if (type == aiTextureType_DIFFUSE)                 texType = TextureType::Diffuse;
			else if (type == aiTextureType_SPECULAR)                texType = TextureType::Specular;
			else if (type == aiTextureType_NORMALS)                 texType = TextureType::Normal;
			else if (type == aiTextureType_HEIGHT)                  texType = TextureType::Height;
			else if (type == aiTextureType_GLTF_METALLIC_ROUGHNESS) texType = TextureType::ARM;
			else continue;

			loadedTextures[path] = std::make_shared<Texture>(path, texType);
		}

		textures.push_back(loadedTextures[path]);
	}

	return textures;
}