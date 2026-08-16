# Shawnty Engine: Product Overview

## What is Shawnty Engine?

**Shawnty Engine** is a lightweight, high-performance 2D game engine designed to make game development faster, cleaner, and more accessible. It bridges the gap between raw execution speed and developer-friendly tools. 

At its core, the engine enforces a strict separation between game logic and graphics rendering. This philosophy ensures that your game code remains clean, modular, and safe from visual glitches, allowing developers to focus purely on what makes their game fun.

## Key Features & Capabilities

### 1. Visual Web Editor (No-Code Scene Building)
Shawnty Engine comes with a modern, built-in visual editor powered by React and FastAPI. You don't need to write layout code or configure coordinates manually. 
- **Drag-and-Drop:** Visually place characters, enemies, and items into your level.
- **Asset Management:** Easily upload and manage textures and animations directly from your browser.
- **Hot-Reloading:** Any changes saved in the web editor instantly update in the running game, providing real-time feedback.

### 2. Python-Powered Gameplay
While the engine itself runs on lightning-fast C++, you don't need to be a C++ expert to make games. 
- All gameplay logic, character movement, and physics interactions are written in **Python**.
- Python's simplicity allows beginners to pick it up quickly, while giving experienced developers the power to prototype features rapidly.

### 3. Data-Driven Design
Say goodbye to hardcoding levels in source code. Shawnty Engine uses simple, human-readable configuration files to define scenes and entities. This allows designers to tweak the game without needing a programmer to recompile the code.

### 4. High-Performance Core
Under the hood, the engine is built using C++17 and standard OpenGL graphics. 
- It handles heavy lifting like physics, rendering pipelines, and memory management with maximum efficiency.
- Your games will run smoothly even on lower-end hardware, and the engine allows you to compile a "Release" version stripped of debug overhead for ultimate performance.

### 5. Multiplayer Ready
The engine was built with the future in mind. It includes a robust architecture for client-server state synchronization, making it significantly easier to develop multiplayer games compared to traditional engines.

### 6. Entity-Component System (ECS)
Shawnty Engine uses an industry-standard ECS architecture. Instead of complex inheritance trees, game objects are built by combining simple modular "components" (like a Transform for position, a Rigidbody for physics, or a Sprite for visuals). This makes adding new features to game objects incredibly flexible.

## Who is it for?
Shawnty Engine is perfect for **indie game developers, hobbyists, and students** who want the blazing-fast performance of a C++ engine combined with the user-friendly experience of a drag-and-drop web editor and Python scripting. It removes the friction from 2D game creation so you can bring your ideas to life faster.
