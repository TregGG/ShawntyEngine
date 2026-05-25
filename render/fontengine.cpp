#include "fontengine.h"
#include <iostream>
#include <glad/glad.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <glm/gtc/type_ptr.hpp>
#define ENGINE_CLASS "FontEngine"
#include "../core/enginedebug.h"

void FontEngine::Init()
{
    SetupShader();

    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);
    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, NULL, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void FontEngine::Shutdown()
{
    if (m_VAO) glDeleteVertexArrays(1, &m_VAO);
    if (m_VBO) glDeleteBuffers(1, &m_VBO);
    if (m_Shader) glDeleteProgram(m_Shader);
    
    for (auto& c : m_Characters) {
        glDeleteTextures(1, &c.second.TextureID);
    }
    m_Characters.clear();
}

bool FontEngine::LoadFont(const std::string& path, unsigned int fontSize)
{
    FT_Library ft;
    if (FT_Init_FreeType(&ft))
    {
        ENGINE_ERROR("Could not init FreeType Library");
        return false;
    }

    FT_Face face;
    if (FT_New_Face(ft, path.c_str(), 0, &face))
    {
        ENGINE_ERROR("Failed to load font %s", path.c_str());
        return false;
    }

    FT_Set_Pixel_Sizes(face, 0, fontSize);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // disable byte-alignment restriction

    for (unsigned char c = 0; c < 128; c++)
    {
        // load character glyph 
        if (FT_Load_Char(face, c, FT_LOAD_RENDER))
        {
            ENGINE_ERROR("Failed to load Glyph %c", c);
            continue;
        }
        // generate texture
        unsigned int texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RED,
            face->glyph->bitmap.width,
            face->glyph->bitmap.rows,
            0,
            GL_RED,
            GL_UNSIGNED_BYTE,
            face->glyph->bitmap.buffer
        );
        // set texture options
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        
        Character character = {
            texture, 
            glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows),
            glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top),
            static_cast<unsigned int>(face->glyph->advance.x)
        };
        m_Characters.insert(std::pair<char, Character>(c, character));
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    
    FT_Done_Face(face);
    FT_Done_FreeType(ft);
    
    ENGINE_LOG("Loaded Font: %s", path.c_str());
    return true;
}

void FontEngine::RenderText(const std::string& text, float x, float y, float scale, const glm::vec3& color, const glm::mat4& projection)
{
    glUseProgram(m_Shader);
    glUniform3f(glGetUniformLocation(m_Shader, "u_TextColor"), color.x, color.y, color.z);
    glUniformMatrix4fv(glGetUniformLocation(m_Shader, "u_Projection"), 1, GL_FALSE, glm::value_ptr(projection));
    glUniform1i(glGetUniformLocation(m_Shader, "u_Text"), 0);
    
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(m_VAO);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    for (auto c = text.begin(); c != text.end(); c++)
    {
        Character ch = m_Characters[*c];

        float xpos = x + ch.Bearing.x * scale;
        float ypos = y - ch.Bearing.y * scale; // Y increases downwards, so subtract bearing

        float w = ch.Size.x * scale;
        float h = ch.Size.y * scale;

        // V=0 at the top, V=1 at the bottom because FreeType bitmaps are top-to-bottom
        float vertices[6][4] = {
            { xpos,     ypos + h,   0.0f, 1.0f }, // bottom-left
            { xpos,     ypos,       0.0f, 0.0f }, // top-left
            { xpos + w, ypos,       1.0f, 0.0f }, // top-right

            { xpos,     ypos + h,   0.0f, 1.0f }, // bottom-left
            { xpos + w, ypos,       1.0f, 0.0f }, // top-right
            { xpos + w, ypos + h,   1.0f, 1.0f }  // bottom-right
        };

        glBindTexture(GL_TEXTURE_2D, ch.TextureID);
        glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
        
        glDrawArrays(GL_TRIANGLES, 0, 6);
        
        x += (ch.Advance >> 6) * scale;
    }
    
    glDisable(GL_BLEND);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

glm::vec2 FontEngine::MeasureText(const std::string& text, float scale)
{
    float w = 0.0f;
    float maxBearing = 0.0f;
    float maxBelow = 0.0f;

    for (auto c = text.begin(); c != text.end(); c++)
    {
        Character ch = m_Characters[*c];
        w += (ch.Advance >> 6) * scale;
        
        float bearingY = ch.Bearing.y * scale;
        float belowY = (ch.Size.y - ch.Bearing.y) * scale;
        
        if (bearingY > maxBearing) maxBearing = bearingY;
        if (belowY > maxBelow) maxBelow = belowY;
    }
    
    return glm::vec2(w, maxBearing + maxBelow);
}

float FontEngine::GetFontAscender(const std::string& text, float scale)
{
    float maxBearing = 0.0f;
    for (auto c = text.begin(); c != text.end(); c++)
    {
        Character ch = m_Characters[*c];
        float bearingY = ch.Bearing.y * scale;
        if (bearingY > maxBearing) maxBearing = bearingY;
    }
    return maxBearing;
}

float FontEngine::GetLineHeight(float scale)
{
    if (m_Characters.find('M') != m_Characters.end()) {
        return m_Characters['M'].Size.y * scale;
    }
    return 24.0f * scale; // Fallback
}

float FontEngine::GetStandardAscender(float scale)
{
    if (m_Characters.find('M') != m_Characters.end()) {
        return m_Characters['M'].Bearing.y * scale;
    }
    return 20.0f * scale; // Fallback
}

void FontEngine::SetupShader()
{
    const char* vertexSrc = R"(#version 330 core
layout (location = 0) in vec4 vertex; // <vec2 pos, vec2 tex>
out vec2 TexCoords;

uniform mat4 u_Projection;

void main()
{
    gl_Position = u_Projection * vec4(vertex.xy, 0.0, 1.0);
    TexCoords = vertex.zw;
}
)";

    const char* fragmentSrc = R"(#version 330 core
in vec2 TexCoords;
out vec4 FragColor;

uniform sampler2D u_Text;
uniform vec3 u_TextColor;

void main()
{    
    vec4 sampled = vec4(1.0, 1.0, 1.0, texture(u_Text, TexCoords).r);
    FragColor = vec4(u_TextColor, 1.0) * sampled;
}
)";

    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexSrc, nullptr);
    glCompileShader(vertexShader);
    
    int success;
    char infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertexShader, 512, nullptr, infoLog);
        ENGINE_ERROR("Font Vertex shader compile error: %s", infoLog);
    }

    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentSrc, nullptr);
    glCompileShader(fragmentShader);

    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragmentShader, 512, nullptr, infoLog);
        ENGINE_ERROR("Font Fragment shader compile error: %s", infoLog);
    }

    m_Shader = glCreateProgram();
    glAttachShader(m_Shader, vertexShader);
    glAttachShader(m_Shader, fragmentShader);
    glLinkProgram(m_Shader);
    
    glGetProgramiv(m_Shader, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(m_Shader, 512, nullptr, infoLog);
        ENGINE_ERROR("Font Shader link error: %s", infoLog);
    }
    
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
}
