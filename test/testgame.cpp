#include "testgame.h"
#include "testscene.h"

#include "../core/engine.h"
#include "../core/input.h"
#include "../assets/assetmanager.h"
#define ENGINE_CLASS "TestGame"
#include "../core/enginedebug.h"

#include <string>
#include <filesystem>
#include <fstream>
#include <GLFW/glfw3.h>

namespace fs = std::filesystem;

// Helper: writes small compiled assets for testing
bool WriteTestCompiledAssets(const std::string& compiledRoot)
{
    try {
        fs::path root(compiledRoot);
        fs::create_directories(root / "textures");
        fs::create_directories(root / "spritesheets");
        fs::create_directories(root / "animations");
        fs::create_directories(root / "objects");

        // --- Write a tiny 2x2 RGBA TGA ---
        std::ofstream tex((root / "textures" / "test.tga").string(), std::ios::binary);
        if (!tex.is_open()) return false;

        unsigned char header[18] = {0};
        header[2] = 2; // uncompressed true-color image
        header[12] = 2; // width low
        header[13] = 0; // width high
        header[14] = 2; // height low
        header[15] = 0; // height high
        header[16] = 32; // bpp
        tex.write(reinterpret_cast<char*>(header), 18);

        // 4 pixels BGRA (2x2)
        unsigned char pixels[16] = {
            0, 0, 255, 255,   // red
            0, 255, 0, 255,   // green
            255, 0, 0, 255,   // blue
            255, 255, 255, 255 // white
        };
        tex.write(reinterpret_cast<char*>(pixels), sizeof(pixels));
        tex.close();

        // --- spritesheet ---
        std::ofstream ss((root / "spritesheets" / "test.ssheet").string());
        ss << "texture:test\n";
        ss << "0: x=0 y=0 w=2 h=2\n";
        ss.close();

        // --- animation set ---
        std::ofstream anim((root / "animations" / "test.anim").string());
        anim << "clip:idle\n";
        anim << "0: frame=0 duration=1.0\n";
        anim.close();

        // --- object descriptor ---
        std::ofstream obj((root / "objects" / "testobj.objasset").string());
        obj << "spritesheet:test\n";
        obj << "animations:test\n";
        obj.close();

        ENGINE_LOG("Wrote test compiled assets to %s", compiledRoot.c_str());
        return true;
    } catch (const std::exception& e) {
        ENGINE_ERROR("WriteTestCompiledAssets error: %s", e.what());
        return false;
    }
}

bool TestGame::OnInit()
{
	ENGINE_LOG("OnInit");

	const std::string compiledRoot = "test_compiled";
	if (!WriteTestCompiledAssets(compiledRoot))
	{
        ENGINE_ERROR("Failed to write compiled assets");
		return false;
	}

    ENGINE_LOG("Assets written, initializing AssetManager");
	if (!m_AssetManager.Initialize(compiledRoot))
	{
        ENGINE_ERROR("AssetManager failed to initialize");
		return false;
	}

    ENGINE_LOG("AssetManager initialized, initializing RenderManager");
	if (!m_RenderManager.Initialize())
	{
        ENGINE_ERROR("RenderManager failed to initialize");
		return false;
	}

    ENGINE_LOG("RenderManager initialized, creating TestScene");
	m_TestScene = new TestScene(&m_AssetManager);

	m_SceneManager.SetInitialScene(m_TestScene);
	m_RenderManager.BindScene(m_TestScene);

    ENGINE_LOG("OnInit completed");
	return true;
}

void TestGame::OnInput(const Input& input)
{
	// Pass input reference to scene for direct handling
	if (m_TestScene)
	{
		m_TestScene->SetInput(input);
	}
}

void TestGame::OnUpdate(float deltaTime)
{
	m_SceneManager.Update(deltaTime);
}

void TestGame::OnRender()
{
	m_RenderManager.BeginFrame();
	m_RenderManager.Render();
	m_RenderManager.EndFrame();
}

void TestGame::OnShutdown()
{   
    m_RenderManager.Shutdown();
    m_SceneManager.Shutdown();
    if (m_TestScene)
    {
        delete m_TestScene;
        m_TestScene = nullptr;
    }

	m_AssetManager.Shutdown();
    ENGINE_LOG("Shutdown");
}

