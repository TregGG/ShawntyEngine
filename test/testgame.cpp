#include "testgame.h"
#include "testscene.h"

#include "../core/engine.h"
#include "../core/input.h"
#include "../assets/assetmanager.h"

#include <iostream>
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

        std::cout << "Wrote test compiled assets to " << compiledRoot << "\n";
        return true;
    } catch (const std::exception& e) {
        std::cerr << "WriteTestCompiledAssets error: " << e.what() << "\n";
        return false;
    }
}

bool TestGame::OnInit()
{
	std::cout << "TestGame: OnInit\n";

	const std::string compiledRoot = "test_compiled";
	if (!WriteTestCompiledAssets(compiledRoot))
	{
		std::cerr << "Failed to write compiled assets\n";
		return false;
	}

	std::cout << "TestGame: Assets written, initializing AssetManager\n";
	if (!m_AssetManager.Initialize(compiledRoot))
	{
		std::cerr << "AssetManager failed to initialize\n";
		return false;
	}

	std::cout << "TestGame: AssetManager initialized, initializing RenderManager\n";
	if (!m_RenderManager.Initialize())
	{
		std::cerr << "RenderManager failed to initialize\n";
		return false;
	}

	std::cout << "TestGame: RenderManager initialized, creating TestScene\n";
	m_TestScene = new TestScene(&m_AssetManager);

	m_SceneManager.SetInitialScene(m_TestScene);
	m_RenderManager.BindScene(m_TestScene);

	std::cout << "TestGame: OnInit completed\n";
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
	std::cout << "TestGame: Shutdown\n";
}

