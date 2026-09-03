#include "../third_party/stb/stb_image.h"

#include "../include/Skybox.h"

#include <iostream>
#include <sstream>
#include <utility>

Skybox::Skybox()
{
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0); 
}

void Skybox::LoadFromFiles(const std::vector<std::string>& faces)
{
    if (ID != 0)
    {
        glDeleteTextures(1, &ID);
        ID = 0;
    }

    stbi_set_flip_vertically_on_load(false);
    glGenTextures(1, &ID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, ID);

    int width, height, nrChannels;
    for (unsigned int i = 0; i < faces.size(); i++)
    {
        unsigned char* data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);
        if (data)
        {
            if (nrChannels == 3)
            {
                glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
            }

            else if (nrChannels == 4)
            {
                glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
            }
        }
        else
        {
            std::cerr << "Cubemap texture failed to load to path: " << faces[i] << std::endl;
        }

        stbi_image_free(data);
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
}

Skybox::~Skybox()
{
    if (VAO != 0) glDeleteVertexArrays(1, &VAO);
    if (VBO != 0) glDeleteBuffers(1, &VBO);
    if (ID != 0) glDeleteTextures(1, &ID);
}

Skybox::Skybox(Skybox&& other) noexcept
    : VAO(std::exchange(other.VAO, 0)),
      VBO(std::exchange(other.VBO, 0)),
      ID(std::exchange(other.ID, 0))
{
}

Skybox& Skybox::operator=(Skybox&& other) noexcept
{
    if (this == &other) return *this;

    if (VAO != 0) glDeleteVertexArrays(1, &VAO);
    if (VBO != 0) glDeleteBuffers(1, &VBO);
    if (ID != 0) glDeleteTextures(1, &ID);

    VAO = std::exchange(other.VAO, 0);
    VBO = std::exchange(other.VBO, 0);
    ID = std::exchange(other.ID, 0);
    return *this;
}

void Skybox::bind() const
{
    glBindVertexArray(VAO);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, ID);
}

void Skybox::draw()
{
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
}
