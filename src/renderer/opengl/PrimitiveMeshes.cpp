#include "hpr/renderer/opengl/PrimitiveMeshes.h"
#include "hpr/renderer/opengl/Mesh.h"

namespace Rendering
{
std::shared_ptr<Mesh> CreateScreenQuad()
{

    std::vector<float> quadVertices = {
        // vertex attributes for a quad that fills the entire screen in Normalized Device Coordinates.
        // positions   // texCoords
        -1.0f, 1.0f, 0.0f, 1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f};

    std::vector<unsigned int> indices = {0, 1, 2, 0, 2, 3};

    std::vector<VertexAttribute> attributes = {{0, 2, GL_FLOAT, GL_FALSE}, {1, 2, GL_FLOAT, GL_FALSE}};

    std::shared_ptr<Geometry> screen = std::make_shared<Geometry>(quadVertices, indices, attributes);
    auto screenQuad = std::make_shared<Mesh>(*screen);
    return screenQuad;
}

std::shared_ptr<Mesh> CreatePlane(const std::vector<std::shared_ptr<Texture>>& textures)
{

    std::vector<float> planeVertices = {
        // pos             // normal         // uv       // tangent       // bitangent
        25,  -5.5f, 25,  0, 1, 0, 25, 0,  1, 0, 0, 0, 0, -1, -25, -5.5f, 25,  0, 1, 0, 0,  0,  1, 0, 0, 0, 0, -1,
        -25, -5.5f, -25, 0, 1, 0, 0,  25, 1, 0, 0, 0, 0, -1, 25,  -5.5f, -25, 0, 1, 0, 25, 25, 1, 0, 0, 0, 0, -1};

    std::vector<unsigned int> p_indices = {0, 1, 2, 0, 2, 3};

    std::vector<VertexAttribute> f_attributes = {{0, 3, GL_FLOAT, GL_FALSE},
                                                 {1, 3, GL_FLOAT, GL_FALSE},
                                                 {2, 2, GL_FLOAT, GL_FALSE},
                                                 {3, 3, GL_FLOAT, GL_FALSE},
                                                 {4, 3, GL_FLOAT, GL_FALSE}};

    std::shared_ptr<Geometry> planeGeo = std::make_shared<Geometry>(planeVertices, p_indices, f_attributes);
    auto plane = std::make_shared<Mesh>(*planeGeo, textures);
    return plane;
}

std::shared_ptr<Mesh> CreateCube()
{

    std::vector<float> cube_vertices{
        // back face
        -1.0f, -1.0f, -1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, // bottom-left   0
        1.0f, 1.0f, -1.0f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f,   // top-right     1
        1.0f, -1.0f, -1.0f, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f,  // bottom-right  2
        -1.0f, 1.0f, -1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f,  // top-left      3
        // front face
        -1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, // bottom-left   4
        1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f,  // bottom-right  5
        1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,   // top-right     6
        -1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f,  // top-left      7
        // left face
        -1.0f, 1.0f, 1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f,   // top-right     8
        -1.0f, 1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f,  // top-left      9
        -1.0f, -1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, // bottom-left   10
        -1.0f, -1.0f, 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f,  // bottom-right  11
                                                            // right face
        1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f,     // top-left      12
        1.0f, -1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f,   // bottom-right  13
        1.0f, 1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f,    // top-right     14
        1.0f, -1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,    // bottom-left   15
        // bottom face
        -1.0f, -1.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, // top-right    16
        1.0f, -1.0f, -1.0f, 0.0f, -1.0f, 0.0f, 1.0f, 1.0f,  // top-left     17
        1.0f, -1.0f, 1.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f,   // bottom-left  18
        -1.0f, -1.0f, 1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f,  // bottom-right 19
        // top face
        -1.0f, 1.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, // top-left     20
        1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f,   // bottom-right 21
        1.0f, 1.0f, -1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f,  // top-right    22
        -1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f   // bottom-left  23
    };

    std::vector<unsigned int> cube_indices = {0,  1,  2,  1,  0,  3,  4,  5,  6,  6,  7,  4,  8,  9,  10, 10, 11, 8,
                                              12, 13, 14, 13, 12, 15, 16, 17, 18, 18, 19, 16, 20, 21, 22, 21, 20, 23};

    std::vector<VertexAttribute> cube_attributes = {
        {0, 3, GL_FLOAT, GL_FALSE}, {1, 3, GL_FLOAT, GL_FALSE}, {2, 2, GL_FLOAT, GL_FALSE}};

    std::shared_ptr<Geometry> cubeGeo = std::make_shared<Geometry>(cube_vertices, cube_indices, cube_attributes);
    auto cube = std::make_shared<Mesh>(*cubeGeo);
    return cube;
}

} // namespace Rendering
