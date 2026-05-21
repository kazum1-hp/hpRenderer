#include "../include/Model.h"
#include "../include/ResourceManager.h"
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
bool Model::reload(const std::string& path)
{
    Model reloaded(path);
    if (reloaded.meshes.empty())
    {
        std::cerr << "Model reload failed, keeping previous model: " << path << std::endl;
        return false;
    }

    meshes = std::move(reloaded.meshes);
    directory = std::move(reloaded.directory);
    loadedTextures = std::move(reloaded.loadedTextures);
    this->path = std::move(reloaded.path);
    instanceCount = reloaded.instanceCount;
    instancingEnabled = reloaded.instancingEnabled;
    return true;
}

bool Model::loadModel(const std::string& path)
{
	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(path,
		aiProcess_Triangulate |
		aiProcess_FlipUVs |
		aiProcess_CalcTangentSpace);

	if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
		std::cerr << "ERROR::ASSIMP:: " << importer.GetErrorString() << std::endl;
		return false;
	}

	this->path = path;

	// Use std::filesystem to get the parent directory, compatible with different path styles.
	std::filesystem::path p(path);
	directory = p.parent_path().string();

	for (unsigned int i = 0; i < scene->mNumMeshes; i++)
	{
		meshes.push_back(processMesh(scene->mMeshes[i], scene));
	}

	return !meshes.empty();
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
		if (!std::filesystem::exists(texpath))
		{
			std::string filename = texpath.filename().string();
			const std::string roughToken = "_rough_";
			const size_t roughPos = filename.find(roughToken);
			if (roughPos != std::string::npos)
			{
				filename.replace(roughPos, roughToken.size(), "_arm_");
				std::filesystem::path fallbackPath = texpath.parent_path() / filename;
				if (std::filesystem::exists(fallbackPath))
				{
					texpath = fallbackPath;
				}
			}
		}
		std::string path = texpath.string();

		std::cout << "[Assimp] Found texture: " << path << " | Type: " << typeName << std::endl;

		if (loadedTextures.find(path) == loadedTextures.end()) 
		{
			TextureType texType;

			if (type == aiTextureType_DIFFUSE ||
				type == aiTextureType_BASE_COLOR)                  texType = TextureType::Diffuse;
			else if (type == aiTextureType_SPECULAR)               texType = TextureType::Specular;
			else if (type == aiTextureType_NORMALS)                texType = TextureType::Normal;
			else if (type == aiTextureType_HEIGHT)                 texType = TextureType::Height;
			else if (type == aiTextureType_GLTF_METALLIC_ROUGHNESS ||
					 type == aiTextureType_METALNESS ||
					 type == aiTextureType_DIFFUSE_ROUGHNESS ||
					 type == aiTextureType_AMBIENT_OCCLUSION)      texType = TextureType::ARM;
			else continue;

			auto texture = ResourceManager::GetInstance().LoadTexture(path, texType);
			if (!texture || !texture->isValid())
			{
				std::cerr << "Skipping invalid model texture: " << path << std::endl;
				continue;
			}
			loadedTextures[path] = texture;
		}

		textures.push_back(loadedTextures[path]);
	}

	return textures;
}
