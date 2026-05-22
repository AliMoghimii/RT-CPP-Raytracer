#pragma once
#include <string>
#include <vector>

class ImageLoader {
public:
    struct ImageData {
        unsigned char* pixels;
        int width;
        int height;
        int channels;
    };

    static int load(const std::string& filename, std::vector<std::string>& texturePaths);

    static ImageData loadPixels(const std::string& filename);
    static void freePixels(ImageData& data);
};