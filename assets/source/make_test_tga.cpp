#include <fstream>

int main()
{
    unsigned char header[18] = {0};
    header[2]  = 2;   // uncompressed true-color
    header[12] = 2;   // width  = 2
    header[13] = 0;
    header[14] = 2;   // height = 2
    header[15] = 0;
    header[16] = 32;  // bits per pixel
    header[17] = 0x20; // top-left origin

    // BGRA format (TGA standard)
    unsigned char pixels[] = {
        0,   0, 255, 255,   // Red
        0, 255,   0, 255,   // Green
        255, 0,   0, 255,   // Blue
        255,255,255,255    // White
    };

    std::ofstream file("test.tga", std::ios::binary);
    file.write((char*)header, 18);
    file.write((char*)pixels, sizeof(pixels));
    file.close();
}
