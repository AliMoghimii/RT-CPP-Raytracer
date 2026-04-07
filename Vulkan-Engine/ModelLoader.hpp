#pragma once

#include <glm/glm.hpp>
#include "GPUData.hpp"
#include <vector>
#include <string>

class ModelLoader {
public:
    static void load(const std::string& filename, std::vector<GPUTriangle>& sceneTriangles, int materialIndex, const glm::vec3& position, const glm::vec3& rotation, float scale);

private:
    static void loadOBJ(const std::string& filename, std::vector<GPUTriangle>& sceneTriangles,int materialIndex, const glm::vec3& position, const glm::vec3& rotation, float scale);
    static glm::vec3 rotateVec(glm::vec3 v, glm::vec3 rot);

};