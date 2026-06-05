#pragma once
#include <glm/glm.hpp>

struct GPUMaterial {
    glm::vec3 color;
    float ambient;
    glm::vec3 emission;
    float diffuse;
    glm::vec3 color2;
    float specular;

    float reflection;
    float transparency;
    float ior;
    int shadingModel;

    int patternType;
    float roughness;
    float metallic;
    int castShadows;

    int useTexture;
    int albedoIndex;
    int normalMapIndex;
    int roughnessIndex;

    int aoIndex;
    int heightMapIndex;
    float proceduralScale;
    float proceduralWobble;

    float bumpStrength;
    float parallaxScale;
    float p5;
    float p6;
};

struct GPUSphere {
    glm::vec3 center;
    float radius;
    int materialIndex;
    float p1;
    float p2;
    float p3;
};

struct GPUTriangle {
    glm::vec3 v0;
    float uv0x;          // tex UV for vertex 0, packed into vec3 padding slots    
    glm::vec3 v1;
    float uv0y;
    glm::vec3 v2;
    float uv1x;
    glm::vec3 n0;
    float uv1y;
    glm::vec3 n1;
    float uv2x;
    glm::vec3 n2;
    int isSmooth;
    int materialIndex;
    float uv2y;
    float p7;
    float p8;
};

struct GPULight {
    glm::vec3 position;
    float radius;
    glm::vec3 color;
    float p2;
};

struct GPUPhoton {
    glm::vec4 position;
    glm::vec4 direction;
    glm::vec4 energy;
};

struct GPUPlane {
    glm::vec3 center;
    float p1;
    glm::vec3 normalVector;
    int materialIndex;
    float p2;
    float p3;
    float p4;
    float p5;
};

struct GPUQuad {
    glm::vec3 corner;
    float p1;
    glm::vec3 edge1;
    float p2;
    glm::vec3 edge2;
    float p3;
    glm::vec3 normalVector;
    int materialIndex;
    float p4;
    float p5;
    float p6;
    float p7;
};

struct GPUCube {
    glm::vec3 boundsMin;
    float p1;
    glm::vec3 boundsMax;
    float p2;
    glm::vec3 center;
    float p3;
    glm::vec3 rotation;
    int materialIndex;
    float p4;
    float p5;
    float p6;
    float p7;
};

struct GPUBVHNode {
    glm::vec3 aabbMin;
    int leftFirst;
    glm::vec3 aabbMax;
    int triCount;
};