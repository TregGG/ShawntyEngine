#include "openglclass.h"

#include "../core/system.h"
#include<glad/glad.h>
#include"camera.h"
#include<iostream>
#include<glm/gtc/matrix_transform.hpp>


 OpenGLClass::OpenGLClass()
{

}

 OpenGLClass::~OpenGLClass()
{

}

bool OpenGLClass::Initialize(System* system, int screenWidth, int screenHeight)
{
    if(!system)
    {
        std::cerr<<"OpenGLClass::Initialize failed: System is null\n";
        return false;
    }

    m_system=system;
    m_screenHeight=screenHeight;
    m_screenWidth=screenWidth;

    glViewport(0,0,m_screenWidth,m_screenHeight);

    glDisable(GL_DEPTH_TEST);


    glClearColor(0.1f,0.1f,0.2f,0.1f);
    if (!InitializeQuad())
        return false;

    if (!InitializeShaders())
        return false;

    std::cout<<"OpenGLClass::Initialize: passed";
    return true;


}

void OpenGLClass::BeginFrame()
{
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
}

void OpenGLClass::EndFrame()
{
    m_system->SwapBuffers();
}

void OpenGLClass::Shutdown()
{
    m_system = nullptr;

}


//Camera functions
void OpenGLClass::SetCamera(Camera* camera)
{
    m_Camera=camera;
}

void OpenGLClass::DrawQuad(const glm::vec2& position)
{
    if (!m_Camera)
        return;

    glm::mat4 model = glm::translate(
        glm::mat4(1.0f),
        glm::vec3(position, 0.0f)
    );

  glm::mat4 mvp =
        m_Camera->GetViewProjection() * model;


    glUseProgram(m_ShaderProgram);

    int loc = glGetUniformLocation(m_ShaderProgram, "u_MVP");
    glUniformMatrix4fv(loc, 1, GL_FALSE, &mvp[0][0]);

    glBindVertexArray(m_QuadVAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr) ;
    //std::cout << "drew quad at " << position.x << " " << position.y << "\n";

}



bool OpenGLClass::InitializeQuad()
{
    float vertices[] = {
        // x, y
        -0.5f, -0.5f,
         0.5f, -0.5f,
         0.5f,  0.5f,
        -0.5f,  0.5f
    };

    unsigned int indices[] = {
        0, 1, 2,
        2, 3, 0
    };

    glGenVertexArrays(1, &m_QuadVAO);
    glGenBuffers(1, &m_QuadVBO);
    glGenBuffers(1, &m_QuadEBO);

    glBindVertexArray(m_QuadVAO);

    glBindBuffer(GL_ARRAY_BUFFER, m_QuadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_QuadEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);


    return true;
}

bool OpenGLClass::InitializeShaders()
{
    const char* vertexSrc = R"(
        #version 330 core
        layout (location = 0) in vec2 a_Position;
        uniform mat4 u_MVP;
        void main()
        {
            gl_Position = u_MVP * vec4(a_Position, 0.0, 1.0);
        }
    )";

    const char* fragmentSrc = R"(
        #version 330 core
        out vec4 FragColor;
        void main()
        {
            FragColor = vec4(1.0, 1.0, 1.0, 1.0);
        }
    )";

    unsigned int vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vertexSrc, nullptr);
    glCompileShader(vs);

    unsigned int fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fragmentSrc, nullptr);
    glCompileShader(fs);

    m_ShaderProgram = glCreateProgram();
    glAttachShader(m_ShaderProgram, vs);
    glAttachShader(m_ShaderProgram, fs);
    glLinkProgram(m_ShaderProgram);

    glDeleteShader(vs);
    glDeleteShader(fs);

    return true;
}
