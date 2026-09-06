#include "Renderer.h"
#include "Scene.h"
#include "InputManager.h"
#include "Editor/ConsoleCapture.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

#ifdef IMGUI_VERSION
#error Runtime headers must not transitively include ImGui
#endif

namespace
{
    void require(bool value, const char* message)
    {
        if (!value) throw std::runtime_error(message);
    }
}

int main()
{
    try
    {
        const IBLSettings ibl;
        require(ibl.environmentSize == 1024 && ibl.irradianceSize == 32 && ibl.prefilterSize == 128
            && ibl.prefilterMipLevels == 5 && ibl.brdfSize == 512, "legacy IBL resolutions");
        Transform transform;
        transform.setInitialTransform(glm::vec3(2.0f), glm::vec3(3.0f));
        transform.setPosition(glm::vec3(7.0f));
        transform.setRotation(glm::vec3(0.5f));
        transform.setScale(glm::vec3(4.0f));
        require(transform.getPosition() == glm::vec3(7.0f), "position getter/setter");
        require(transform.getRotation() == glm::vec3(0.5f), "rotation getter/setter");
        require(transform.getScale() == glm::vec3(4.0f), "scale getter/setter");
        transform.resetRotation();
        require(transform.getRotation() == glm::vec3(0.0f), "rotation reset");
        require(transform.getPosition() == glm::vec3(7.0f), "rotation reset moved object");
        transform.reset();
        require(transform.getPosition() == glm::vec3(2.0f), "initial position reset");
        require(transform.getScale() == glm::vec3(3.0f), "initial scale reset");

        Light light(glm::vec3(1.0f), 1.0f, glm::vec3(0.0f), LightType::Point);
        light.setEnabled(false);
        light.setColor(glm::vec3(0.5f));
        light.setLightPos(glm::vec3(2.0f));
        light.setLightDir(glm::vec3(-1.0f));
        light.setIntensity(3.0f);
        require(!light.lightOn() && light.getIntensity() == 3.0f, "light editing");
        require(light.getColor() == glm::vec3(0.5f) && light.getLightPos() == glm::vec3(2.0f)
            && light.getLightDir() == glm::vec3(-1.0f), "light vector editing");

        // No ImGui or GLFW context is created. Input callbacks consume plain policy data.
        Camera camera;
        InputManager input(camera);
        input.setMoveSpeed(23.0f);
        require(input.getMoveSpeed() == 23.0f, "input speed editing");
        const auto initialFov = camera.getFov();
        input.onScroll(0, 1);
        require(camera.getFov() == initialFov, "scroll outside viewport");
        input.setCaptureState({true, true, true});
        input.onScroll(0, 1);
        input.onMouseMove(100, 100);
        const auto front = camera.getFront();
        input.onMouseMove(200, 200);
        require(camera.getFov() == initialFov && camera.getFront() == front, "UI captured mouse");
        input.setCaptureState({false, false, true});
        input.onScroll(0, 1);
        require(camera.getFov() < initialFov, "viewport scroll");
        input.onMouseMove(100, 100);
        input.onMouseMove(110, 100);
        require(camera.getFront() != front, "viewport mouse movement");

        auto* originalOut = std::cout.rdbuf();
        auto* originalError = std::cerr.rdbuf();
        {
            ConsoleCapture outer;
            std::cout << "first";
            std::cerr << "error";
            require(outer.text() == "firsterror", "stdout/stderr capture");
            {
                ConsoleCapture inner;
                std::cout << "inner";
                require(inner.text() == "inner", "nested capture");
            }
            std::cout << "after";
            require(outer.text() == "firsterrorafter", "nested restoration");
            outer.clear();
            require(outer.text().empty(), "console clear");
            std::cerr << "again";
            require(outer.text() == "again", "capture after clear");
        }
        require(std::cout.rdbuf() == originalOut && std::cerr.rdbuf() == originalError, "stream restoration");
        std::cout << "Runtime boundary tests passed (no ImGui context or linkage).\n";
        return 0;
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
