#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "ImageLoader.hpp"
#include <stdexcept>
#include <iostream>

int ImageLoader::load(const std::string& filename, std::vector<std::string>& texturePaths) {
    texturePaths.push_back(filename);
    std::cout << "ImageLoader: Loaded " << filename << " at index " << (texturePaths.size() - 1) << "\n";
    return static_cast<int>(texturePaths.size() - 1);
}

ImageLoader::ImageData ImageLoader::loadPixels(const std::string& filename) {
    ImageData data;
    data.pixels = stbi_load(filename.c_str(), &data.width, &data.height, &data.channels, STBI_rgb_alpha);
    if (!data.pixels) {
        throw std::runtime_error("Error ImageLoader: failed to load " + filename);
    }
    return data;
}

void ImageLoader::freePixels(ImageData& data) {
    stbi_image_free(data.pixels);
}