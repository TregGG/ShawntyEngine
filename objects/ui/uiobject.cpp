#include "uiobject.h"
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>

// Static renderer state for UI backgrounds
static unsigned int s_UIVAO = 0;
static unsigned int s_UIVBO = 0;
static unsigned int s_UIShader = 0;

UIObject::UIObject(Scene* scene, const std::string& name)
    : GameObject(scene, name)
{
    if (s_UIVAO == 0)
    {
        // Setup simple quad and shader for UI backgrounds
        float vertices[] = {
            // pos      // tex
            0.0f, 1.0f, 0.0f, 1.0f,
            0.0f, 0.0f, 0.0f, 0.0f,
            1.0f, 0.0f, 1.0f, 0.0f,

            0.0f, 1.0f, 0.0f, 1.0f,
            1.0f, 0.0f, 1.0f, 0.0f,
            1.0f, 1.0f, 1.0f, 1.0f
        };
        glGenVertexArrays(1, &s_UIVAO);
        glGenBuffers(1, &s_UIVBO);
        glBindVertexArray(s_UIVAO);
        glBindBuffer(GL_ARRAY_BUFFER, s_UIVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        const char* vShader = R"(#version 330 core
            layout (location = 0) in vec4 vertex;
            out vec2 TexCoords;
            uniform mat4 u_Model;
            uniform mat4 u_Projection;
            void main() {
                gl_Position = u_Projection * u_Model * vec4(vertex.xy, 0.0, 1.0);
                TexCoords = vertex.zw;
            }
        )";
        const char* fShader = R"(#version 330 core
            in vec2 TexCoords;
            out vec4 FragColor;
            uniform vec4 u_Color;
            uniform sampler2D u_Tex;
            uniform bool u_UseTex;
            void main() {
                if(u_UseTex) FragColor = texture(u_Tex, TexCoords) * u_Color;
                else FragColor = u_Color;
            }
        )";
        unsigned int vs = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vs, 1, &vShader, NULL);
        glCompileShader(vs);
        unsigned int fs = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fs, 1, &fShader, NULL);
        glCompileShader(fs);
        s_UIShader = glCreateProgram();
        glAttachShader(s_UIShader, vs);
        glAttachShader(s_UIShader, fs);
        glLinkProgram(s_UIShader);
        glDeleteShader(vs);
        glDeleteShader(fs);
    }
}

void UIObject::Update(float dt)
{
    if (!Active) return;
    for (auto& child : m_Children) {
        child->Update(dt);
    }
}

void UIObject::Render(const glm::mat4& projection)
{
    if (!Active) return;
    RenderBackground(projection);
    for (auto& child : m_Children) {
        child->Render(projection);
    }
}

void UIObject::AddChild(std::unique_ptr<UIObject> child)
{
    child->m_Parent = this;
    m_Children.push_back(std::move(child));
}

void UIObject::RenderBackground(const glm::mat4& projection)
{
    if (BackgroundColor.a <= 0.0f && BackgroundTexture == 0) return;

    glUseProgram(s_UIShader);
    
    // Absolute position calculation (simple relative-to-parent logic)
    glm::vec2 absPos = Position;
    UIObject* currParent = m_Parent;
    while (currParent) {
        absPos += currParent->Position;
        currParent = currParent->m_Parent;
    }

    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(absPos, 0.0f));
    model = glm::scale(model, glm::vec3(Size, 1.0f));

    glUniformMatrix4fv(glGetUniformLocation(s_UIShader, "u_Model"), 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(glGetUniformLocation(s_UIShader, "u_Projection"), 1, GL_FALSE, glm::value_ptr(projection));
    glUniform4f(glGetUniformLocation(s_UIShader, "u_Color"), BackgroundColor.r, BackgroundColor.g, BackgroundColor.b, BackgroundColor.a);
    
    if (BackgroundTexture != 0) {
        glUniform1i(glGetUniformLocation(s_UIShader, "u_UseTex"), 1);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, BackgroundTexture);
        glUniform1i(glGetUniformLocation(s_UIShader, "u_Tex"), 0);
    } else {
        glUniform1i(glGetUniformLocation(s_UIShader, "u_UseTex"), 0);
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    glBindVertexArray(s_UIVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}
