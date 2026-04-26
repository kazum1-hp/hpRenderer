#pragma once

#include <vector>
#include <string>
#include <map>
#include <memory>

#include "Mesh.h"
#include "Texture.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

class Model
{
public:
	std::vector<std::unique_ptr<Mesh>> meshes;
	std::string directory;
	//std::vector<std::shared_ptr<Texture>> textures;
	unsigned int instanceCount = 0;
	bool instancingEnabled = false;

	Model(const std::string& path);
	void draw() const;

	void enableInstancing(const std::vector<glm::mat4>& instanceTransforms);

	void reload(const std::string& path);

private:
	void loadModel(const std::string& path);
	std::unique_ptr<Mesh> processMesh(aiMesh* mesh, const aiScene* scene);
	std::vector<std::shared_ptr<Texture>> loadMaterialTextures(aiMaterial* mat, aiTextureType type, const std::string& typeName);

	std::map<std::string, std::shared_ptr<Texture>> loadedTextures;
};

