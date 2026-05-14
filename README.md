# Framework Engine

Welcome to the Framework Engine! This is a lightweight 2D game engine written in C++17. It is designed to be simple, fast, and easy to use. The engine keeps game logic and rendering completely separate, making your game code cleaner and less prone to graphics-related bugs.

## Getting Started

### Prerequisites

You need a C++17 compatible compiler and standard OpenGL/GLFW libraries installed.

**On Ubuntu/Debian:**
```bash
sudo apt-get install build-essential libglfw3-dev libgl1-mesa-dev pkg-config
```

### Building the Engine
You can compile the engine using `make`.

```bash
# Compile in parallel (fastest)
make

# Run the compiled engine
./bin/framework
```

**Build Options:**
- `make BUILD=debug` (default): Compiles with extra logging to help you find bugs. Logs are printed to the console and saved to `logs.txt`.
- `make BUILD=console`: Logs only to the console.
- `make BUILD=file`: Logs only to `logs.txt`.
- `make BUILD=release`: Compiles without any logging for maximum performance. Use this when you are ready to publish your game!

**Cleaning up:**
If you ever want to rebuild everything from scratch, run `make clean` first.

## Creating Your First Game

Making a game involves extending the `Game` and `Scene` classes, and creating `GameObjects`.

1. **Set up the Game Loop:**
The `Game` class handles your main lifecycle hooks (init, update, render).
```cpp
class MyGame : public Game {
public:
    bool OnInit() override;
    void OnInput(const Input& input) override;
    void OnUpdate(float deltaTime) override;
    void OnRender() override;
    void OnShutdown() override;
};
```

2. **Create a Scene:**
A `Scene` is where all your game objects live (like a specific level or menu screen).
```cpp
class MyLevel : public Scene {
public:
    MyLevel(AssetManager* assets) : Scene(assets) {}

    void OnEnter() override {
        // Create objects when the scene loads
        auto player = std::make_unique<GameObject>("Player");
        player->GetTransform().position = glm::vec2(100.0f, 100.0f);
        m_GameObjects.push_back(std::move(player));
    }
    
    void OnExit() override {}
    void Update(float deltatime) override {
        // Update components
        for(auto& obj : m_GameObjects) {
            obj->Update(deltatime);
        }
    }
};
```

3. **Start the Engine:**
In your `main.cpp`, initialize the engine with your game and run it!
```cpp
int main() {
    Engine engine;
    MyGame game;

    if (!engine.Initialize(&game)) {
        return -1;
    }
    
    engine.Run();
    engine.Shutdown();
    return 0;
}
```

## Documentation

Want to learn more? Check out the detailed guides in the `documentation/` folder:

- **[Entity-Component System](documentation/components.md)**: How to make GameObjects and custom Behaviors.
- **[Physics & Collisions](documentation/physics.md)**: How bounding boxes, rigid bodies, and collisions work.
- **[Rendering Pipeline](documentation/rendering.md)**: How sprites and graphics are drawn.
- **[Entity Registry](documentation/registry.md)**: Grouping and finding game objects easily.
- **[Scene Management](documentation/scene.md)**: How levels and game screens work.
- **[Asset Manager](documentation/assetmanager.md)**: Loading textures and animations.
- **[Logging System](documentation/logging.md)**: How to track bugs and print messages.

## Contributing

Contributions are welcome! Please ensure code follows the existing patterns:
- Member variables use `m_` prefix.
- No OpenGL calls in gameplay code (use the RenderManager).
- Use `std::unique_ptr` to manage object ownership safely.
