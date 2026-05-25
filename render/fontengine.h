#pragma once
#include <map>
#include <string>
#include <glm/glm.hpp>
#include "../services/service.h"

struct Character {
    unsigned int TextureID; // ID handle of the glyph texture
    glm::ivec2   Size;      // Size of glyph
    glm::ivec2   Bearing;   // Offset from baseline to left/top of glyph
    unsigned int Advance;   // Horizontal offset to advance to next glyph
};

class FontEngine : public Service
{
public:
    void Init() override;
    void Shutdown() override;

    bool LoadFont(const std::string& path, unsigned int fontSize);
    
    // Renders text using the loaded font
    void RenderText(const std::string& text, float x, float y, float scale, const glm::vec3& color, const glm::mat4& projection);

    // Layout helpers
    glm::vec2 MeasureText(const std::string& text, float scale);
    float GetFontAscender(const std::string& text, float scale);
    float GetLineHeight(float scale);
    float GetStandardAscender(float scale);

private:
    std::map<char, Character> m_Characters;
    unsigned int m_VAO = 0, m_VBO = 0;
    unsigned int m_Shader = 0;
    
    void SetupShader();
};
