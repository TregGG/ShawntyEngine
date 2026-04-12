#define GLFW_INCLUDE_NONE
#include <glad/glad.h>

#include "spriterendererclass.h"
#include <glm/glm.hpp>
#define ENGINE_CLASS "SpriteRendererClass"
#include "../core/enginedebug.h"


bool SpriteRendererClass::Initialize()
{
    if (m_Initialized)
        return true;

    SetupQuad();
    SetupShader();
    SetupDebugQuad();
    SetupDebugShader();

    m_Initialized = true;
    return true;
}

void SpriteRendererClass::SetupQuad()
{
    float vertices[16] = {
        -0.5f, -0.5f,  0.0f, 0.0f,
         0.5f, -0.5f,  1.0f, 0.0f,
         0.5f,  0.5f,  1.0f, 1.0f,
        -0.5f,  0.5f,  0.0f, 1.0f
    };

    unsigned int indices[6] = { 0, 1, 2, 2, 3, 0 };

    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);
    glGenBuffers(1, &m_EBO);

    glBindVertexArray(m_VAO);

    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

void SpriteRendererClass::DrawSprite(const TextureGPU& texture,
                                     const SpriteFrame& frame,
                                     const glm::mat4& mvp)
{
    // calculate UVs from frame
    float u0 = (float)frame.x / texture.width;
    float v0 = (float)frame.y / texture.height;
    float u1 = (float)(frame.x + frame.w) / texture.width;
    float v1 = (float)(frame.y + frame.h) / texture.height;

    float vertices[16] = {
        -0.5f, -0.5f,  u0, v1,
         0.5f, -0.5f,  u1, v1,
         0.5f,  0.5f,  u1, v0,
        -0.5f,  0.5f,  u0, v0
    };

    glUseProgram(m_Shader);

    glUniformMatrix4fv(glGetUniformLocation(m_Shader, "u_MVP"),
                       1, GL_FALSE, &mvp[0][0]);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture.handle);
    glUniform1i(glGetUniformLocation(m_Shader, "u_Texture"), 0);

    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);

    glBindVertexArray(m_VAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void SpriteRendererClass::DrawDebugRect(const glm::mat4& mvp, const glm::vec3& color)
{
    glUseProgram(m_DebugShader);
    glUniformMatrix4fv(glGetUniformLocation(m_DebugShader, "u_MVP"), 1, GL_FALSE, &mvp[0][0]);
    glUniform3f(glGetUniformLocation(m_DebugShader, "u_Color"), color.r, color.g, color.b);

    glBindVertexArray(m_DebugVAO);
    glDrawArrays(GL_LINE_LOOP, 0, 4);
    glBindVertexArray(0);
}

void SpriteRendererClass::SetupShader()
{
    const char* vertexSrc = R"(#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aUV;

uniform mat4 u_MVP;

out vec2 vUV;

void main()
{
    vUV = aUV;
    gl_Position = u_MVP * vec4(aPos, 0.0, 1.0);
}
)";

    const char* fragmentSrc = R"(#version 330 core
in vec2 vUV;

uniform sampler2D u_Texture;

out vec4 FragColor;

void main()
{
    FragColor = texture(u_Texture, vUV);
}
)";

    // ---------------- Vertex Shader ----------------
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexSrc, nullptr);
    glCompileShader(vertexShader);

    int success;
    char infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(vertexShader, 512, nullptr, infoLog);
        ENGINE_ERROR("Vertex shader compile error: %s", infoLog);
    }

    // ---------------- Fragment Shader ----------------
    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentSrc, nullptr);
    glCompileShader(fragmentShader);

    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fragmentShader, 512, nullptr, infoLog);
        ENGINE_ERROR("Fragment shader compile error: %s", infoLog);
    }

    // ---------------- Shader Program ----------------
    m_Shader = glCreateProgram();
    glAttachShader(m_Shader, vertexShader);
    glAttachShader(m_Shader, fragmentShader);
    glLinkProgram(m_Shader);

    glGetProgramiv(m_Shader, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetProgramInfoLog(m_Shader, 512, nullptr, infoLog);
        ENGINE_ERROR("Shader link error: %s", infoLog);
    }

    // shaders no longer needed after linking
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
}

void SpriteRendererClass::SetupDebugQuad()
{
    float lineVertices[8] = {
        -0.5f, -0.5f,
         0.5f, -0.5f,
         0.5f,  0.5f,
        -0.5f,  0.5f
    };

    glGenVertexArrays(1, &m_DebugVAO);
    glGenBuffers(1, &m_DebugVBO);

    glBindVertexArray(m_DebugVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_DebugVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(lineVertices), lineVertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
}

void SpriteRendererClass::SetupDebugShader()
{
    const char* vertexSrc = R"(#version 330 core
layout (location = 0) in vec2 aPos;
uniform mat4 u_MVP;
void main() { gl_Position = u_MVP * vec4(aPos, 0.0, 1.0); }
)";

    const char* fragmentSrc = R"(#version 330 core
uniform vec3 u_Color;
out vec4 FragColor;
void main() { FragColor = vec4(u_Color, 1.0); }
)";

    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexSrc, nullptr);
    glCompileShader(vertexShader);

    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentSrc, nullptr);
    glCompileShader(fragmentShader);

    m_DebugShader = glCreateProgram();
    glAttachShader(m_DebugShader, vertexShader);
    glAttachShader(m_DebugShader, fragmentShader);
    glLinkProgram(m_DebugShader);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
}

void SpriteRendererClass::Shutdown()
{
    if (m_VAO)    glDeleteVertexArrays(1, &m_VAO);
    if (m_VBO)    glDeleteBuffers(1, &m_VBO);
    if (m_EBO)    glDeleteBuffers(1, &m_EBO);
    if (m_Shader) glDeleteProgram(m_Shader);
    
    if (m_DebugVAO) glDeleteVertexArrays(1, &m_DebugVAO);
    if (m_DebugVBO) glDeleteBuffers(1, &m_DebugVBO);
    if (m_DebugShader) glDeleteProgram(m_DebugShader);

    m_VAO = m_VBO = m_EBO = m_Shader = m_DebugVAO = m_DebugVBO = m_DebugShader = 0;
}

