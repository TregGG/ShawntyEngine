#include <filesystem>
#include <fstream>
#include <vector>
#include <string>
#include <iostream>

namespace fs = std::filesystem;

// Creates a minimal compiled-assets layout used by the tests:
// compiled_root/
//   textures/test.tga
//   spritesheets/test.ssheet
//   animations/test.anim
//   objects/testobj.objasset

// bool WriteTestCompiledAssets(const std::string& compiledRoot)
// {
//     try {
//         fs::path root(compiledRoot);
//         fs::create_directories(root / "textures");
//         fs::create_directories(root / "spritesheets");
//         fs::create_directories(root / "animations");
//         fs::create_directories(root / "objects");

//         // --- Write a tiny 2x2 RGBA TGA ---
//         std::ofstream tex((root / "textures" / "test.tga").string(), std::ios::binary);
//         if (!tex.is_open()) return false;

//         unsigned char header[18] = {0};
//         header[2] = 2; // uncompressed true-color image
//         header[12] = 2; // width low
//         header[13] = 0; // width high
//         header[14] = 2; // height low
//         header[15] = 0; // height high
//         header[16] = 32; // bpp
//         tex.write(reinterpret_cast<char*>(header), 18);

//         // 4 pixels BGRA (2x2)
//         unsigned char pixels[16] = {
//             // pixel 0 (B,G,R,A)
//             0, 0, 255, 255,   // red
//             0, 255, 0, 255,   // green
//             255, 0, 0, 255,   // blue
//             255, 255, 255, 255 // white
//         };
//         tex.write(reinterpret_cast<char*>(pixels), sizeof(pixels));
//         tex.close();

//         // --- spritesheet --- refer to texture id 'test' and a single frame ---
//         std::ofstream ss((root / "spritesheets" / "test.ssheet").string());
//         ss << "texture:test\n";
//         ss << "0: x=0 y=0 w=2 h=2\n";
//         ss.close();

//         // --- animation set --- one clip with one frame ---
//         std::ofstream anim((root / "animations" / "test.anim").string());
//         anim << "clip:idle\n";
//         anim << "0: frame=0 duration=1.0\n";
//         anim.close();

//         // --- object descriptor mapping object id 'testobj' ---
//         std::ofstream obj((root / "objects" / "testobj.objasset").string());
//         obj << "spritesheet:test\n";
//         obj << "animations:test\n";
//         obj.close();

//         std::cout << "Wrote test compiled assets to " << compiledRoot << "\n";
//         return true;
//     } catch (const std::exception& e) {
//         std::cerr << "WriteTestCompiledAssets error: " << e.what() << "\n";
//         return false;
//     }
// }
