#include "sanitycheck.h"
#include "core/input.h"
#include <glad/glad.h>
#include"render/camera.h"

#include <iostream>
#include <GLFW/glfw3.h>

#include<fstream>
#include <glm/gtc/matrix_transform.hpp>



static glm::vec2 g_MoveDir { 0.0f, 0.0f };
static glm::vec2 g_CameraDir { 0.0f, 0.0f };

static TextureGPU LoadTGAtoGPU(const char* path)
{
    std::ifstream file(path, std::ios::binary);

    unsigned char header[18];
    file.read((char*)header, 18);

    int width  = header[12] | (header[13] << 8);
    int height = header[14] | (header[15] << 8);
    int bpp    = header[16];
    int channels = bpp / 8;

    int imageSize = width * height * channels;
    unsigned char* data = new unsigned char[imageSize];
    file.read((char*)data, imageSize);
    file.close();

    unsigned int tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexImage2D(GL_TEXTURE_2D, 0,
                 GL_RGBA,
                 width, height, 0,
                 GL_BGRA, GL_UNSIGNED_BYTE,
                 data);

    delete[] data;

    return { tex, width, height };
}


bool SanityGame::OnInit()
{
   std::cout << "[SanityGame] Initialized\n";
    return true;
}

void SanityGame::OnInput(const Input& input)
{
    g_MoveDir = { 0.0f, 0.0f };
    g_CameraDir = { 0.0f, 0.0f };

    // ---- WASD → move quad ----
    if (input.IsKeyDown(GLFW_KEY_W)) g_MoveDir.y += 1.0f;
    if (input.IsKeyDown(GLFW_KEY_S)) g_MoveDir.y -= 1.0f;
    if (input.IsKeyDown(GLFW_KEY_A)) g_MoveDir.x -= 1.0f;
    if (input.IsKeyDown(GLFW_KEY_D)) g_MoveDir.x += 1.0f;

    // ---- Arrow keys → move camera ----
    if (input.IsKeyDown(GLFW_KEY_UP))    g_CameraDir.y += 1.0f;
    if (input.IsKeyDown(GLFW_KEY_DOWN))  g_CameraDir.y -= 1.0f;
    if (input.IsKeyDown(GLFW_KEY_LEFT))  g_CameraDir.x -= 1.0f;
    if (input.IsKeyDown(GLFW_KEY_RIGHT)) g_CameraDir.x += 1.0f;
}

void SanityGame::OnUpdate(float deltaTime)
{
    // Move quad
    if (g_MoveDir.x != 0.0f || g_MoveDir.y != 0.0f)
    {
        m_PlayerPos += g_MoveDir * m_MoveSpeed * deltaTime;
    }

    // Move camera
    if (g_CameraDir.x != 0.0f || g_CameraDir.y != 0.0f)
    {
        glm::vec2 camPos = m_Camera->GetCameraPosition();
        camPos += g_CameraDir * m_MoveSpeed * deltaTime;
        m_Camera->SetCameraPosition(camPos.x, camPos.y);
    }
}

void SanityGame::OnRender()
{
    if (!m_RendererReady)
    {
        m_SpriteRenderer.Initialize();

        m_TestTexture = LoadTGAtoGPU("assets/source/test.tga");

        m_TestFrame.x = 0;
        m_TestFrame.y = 0;
        m_TestFrame.w = m_TestTexture.width;
        m_TestFrame.h = m_TestTexture.height;

        m_Camera->SetViewSize(10.0f, 7.5f);
        m_Camera->SetCameraPosition(0.0f, 0.0f);
     //   std::cout << "Camera ptr: " << m_Camera << std::endl;


        m_RendererReady = true;
    }

    glClearColor(0.1f, 0.1f, 0.1f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

// Model transform
    glm::mat4 model = glm::translate(glm::mat4(1.0f),
                                 glm::vec3(m_PlayerPos, 0.0f));
    model = glm::scale(model, glm::vec3(2.0f));

// MVP using YOUR camera
    glm::mat4 mvp = m_Camera->GetViewProjection() * model;

    m_SpriteRenderer.DrawSprite(
        m_TestTexture,
        m_TestFrame,
        mvp
    );

}


void SanityGame::OnShutdown()
{
    m_SpriteRenderer.Shutdown();
    std::cout << "[SanityGame] Shutdown\n";
}



