#pragma once
#include "Shader.h"
#include "Model.h"
#include "FrameBuffer.h"
#include "Skybox.h"
#include "Camera.h"
#include "Light.h"
#include "Material.h"
#include "InputManager.h"
#include "Window.h"
#include "Transform.h"
#include "../third_party/imgui/imgui.h"
#include "../third_party/imgui/imgui_internal.h"
#include "../third_party/ImGuiFileDialog/ImGuiFileDialog.h"
#include "../third_party/ImGuiFileDialog/ImGuiFileDialogConfig.h"

#include <vector>
#include <memory>
#include <sstream>

class Scene;
struct Environment;
class ResourceManager;

class Renderer
{
private:
	// --- External reference ---
	Window& window;
	Camera& camera;
	InputManager& input;
	Scene& scene;

	// --- Pipeline Resource ---
	// The renderer needs to hold the shaders it uses to perform rendering tasks, but the data is obtained from the ResourceManager.
	std::shared_ptr<Shader> modelShader;
	std::shared_ptr<Shader> lightShader;
	std::shared_ptr<Shader> sceneFramebufferShader;
	std::shared_ptr<Shader> skyboxShader;
	std::shared_ptr<Shader> dirShadowShader;
	std::shared_ptr<Shader> pointShadowShader;
	std::shared_ptr<Shader> bloomBlurShader;
	std::shared_ptr<Shader> gBufferShader;
	std::shared_ptr<Shader> lightPassShader;
	std::shared_ptr<Shader> debugShader;
	std::shared_ptr<Shader> gbufferDebugShader;
	std::shared_ptr<Shader> backgroundShader;
	std::shared_ptr<Shader> irradianceShader;
	std::shared_ptr<Shader> prefilterShader;
	std::shared_ptr<Shader> brdfShader;

	// framebuffers
	std::vector<std::unique_ptr<FrameBuffer>> framebuffers;
	std::unique_ptr<FrameBuffer> pingpongFrameBuffer[2];
	std::vector<std::unique_ptr<FrameBuffer>> pointShadowFramebuffers;

	std::shared_ptr<Mesh> screenQuad;
	std::shared_ptr<Mesh> plane;
	std::shared_ptr<Mesh> cube;

	static std::stringstream buffer;

	glm::mat4 LightSpaceMatrix;

	//unsigned int envCubemap = 0;
	//unsigned int irradianceMap = 0;
	//unsigned int prefilterMap = 0;
	//unsigned int brdfLUTTexture = 0;

	int effectMode = 0;
	int toneMappingMode = 0;
	float offset = 300.0f;

	float scanPos = 0.0f;

	//glm::vec3 normalColor;

	//bool enableInstancing = false;
	//int instance = 1;

	bool useMSAA = true;
	bool useBlinnPhong = true;
	bool useQuadratic = true;

	const unsigned int SHADOW_Size = 1024;
	const unsigned int resolution = 1024;

	bool parallelShadows = true;
	bool pointShadows = true;
	bool useShadows = false;

	bool hasNormal = false;
	bool hasHeight = false;
	float height_scale = 0.001f;
	bool hasARMMap = false;

	float samplerDistance = 1.0f;
	bool useHdr = true;
	bool useBloom = false;
	float exposure = 1.0f;

	float aoBias = 0.0f;
	float roughnessBias = 0.0f;
	float metallicBias = 0.0f;

	bool useDeferred = false;
	bool usePostProcess = false;
	bool drawLights = false;
	bool drawPlane = false;
	bool drawDebug = false;

	int framebufferWidth = 0;
	int framebufferHeight = 0;
	unsigned int finalTexture = 0;
	// ------------------------------------------------------------------------------
	static void releaseEnvironmentMaps(Environment& env);
	void prepareEnvironment(Environment& env);
	void generateIBLMaps(Environment& env);

	void renderModel(const Transform& transform, const Model& model, Shader& shader);
	void drawMesh(const Mesh& mesh, Shader& shader) const;
	void drawModel(const Model& model, Shader& shader) const;
	void resizeFrameBuffer(unsigned int newWidth, unsigned int newHeight);

	void forwardPass(Scene& scene);
	void deferredPass(Scene& scene);
	void shadowPass(Scene& scene);
	void geometryPass(Scene& scene);
	void lightPass(Scene& scene);
	void postProcessPass(const FrameBuffer& framebuffer);

public:
	Renderer(Camera& cam, InputManager& input, Window& win, Scene& scene);
	~Renderer();
	void init();
	void render(Scene& scene);
	static void InitConsole();
	void onImGuiRender();
};

