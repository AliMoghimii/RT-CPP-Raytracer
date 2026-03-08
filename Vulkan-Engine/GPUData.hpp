#pragma once
#include <glm/glm.hpp>

struct GPUMaterial {
    glm::vec3 color;
    float ambient;

    float diffuse;
    float specular;
    float reflection;
    float transparency;

    float ior;
    int type;
    float padding[2];
};

struct GPUSphere {
    glm::vec3 center;
    float radius;

    int materialIndex;
    float padding[3];
};

struct GPUTriangle {
    glm::vec3 v0;
    float padding1;
    glm::vec3 v1;
    float padding2;
    glm::vec3 v2;
    float padding3;

    glm::vec3 n0;
    float padding4;
    glm::vec3 n1;
    float padding5;
    glm::vec3 n2;
    int isSmooth;

    int materialIndex;
    float padding6[3];
};