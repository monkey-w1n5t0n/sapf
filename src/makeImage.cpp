#include "makeImage.hpp"
#include <vector>
#include <cstring>
#include <iostream>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

struct Bitmap {
    int width;
    int height;
    std::vector<unsigned char> pixels; // RGBA
};

Bitmap* createBitmap(int width, int height) {
    Bitmap* bmp = new Bitmap();
    bmp->width = width;
    bmp->height = height;
    bmp->pixels.resize(width * height * 4, 0); // Initialize to transparent black or white? Let's say 0.
    return bmp;
}

void freeBitmap(Bitmap* bitmap) {
    delete bitmap;
}

void setPixel(Bitmap* bitmap, int x, int y, int r, int g, int b, int a) {
    if (!bitmap || x < 0 || x >= bitmap->width || y < 0 || y >= bitmap->height) return;
    size_t index = (y * bitmap->width + x) * 4;
    bitmap->pixels[index] = (unsigned char)r;
    bitmap->pixels[index + 1] = (unsigned char)g;
    bitmap->pixels[index + 2] = (unsigned char)b;
    bitmap->pixels[index + 3] = (unsigned char)a;
}

void fillRect(Bitmap* bitmap, int x, int y, int width, int height, int r, int g, int b, int a) {
    if (!bitmap) return;
    // Simple implementation, optimization possible but likely not needed
    for (int j = y; j < y + height; ++j) {
        for (int i = x; i < x + width; ++i) {
            setPixel(bitmap, i, j, r, g, b, a);
        }
    }
}

void writeBitmap(Bitmap* bitmap, const char *path) {
    if (!bitmap || !path) return;
    // Assume PNG for now as default, or detect from extension?
    // The original code likely used JPEG or PNG based on context.
    // stbi_write_png handles alpha channel well.
    
    stbi_write_png(path, bitmap->width, bitmap->height, 4, bitmap->pixels.data(), bitmap->width * 4);
}
