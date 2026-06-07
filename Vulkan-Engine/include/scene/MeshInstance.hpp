#pragma once
#include "scene/GPUData.hpp"
#include "scene/Mesh.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <limits>

// A lightweight placement of a Mesh (BLAS) in the world: just a transform + material
// override. Animating the object means recomputing this struct's GPUInstance mirror each
// frame (cheap mat4 math) -- the underlying BLAS triangles/BVH never move or rebuild.
struct MeshInstance {
    int meshIndex = 0;                  // index into Renderer::sceneMeshes (not a pointer -- vector-safe)
    glm::vec3 basePosition{ 0.0f };
    glm::vec3 baseRotationDeg{ 0.0f };
    float scale = 1.0f;
    float spinDegPerSec = 0.0f;         // continuous spin around world Y axis
    float bobAmplitude = 0.0f;          // vertical bob half-range, world units (0 = no bob)
    float bobSpeed = 0.0f;              // bob oscillations per second (Hz)
    int materialOverride = -1;          // -1 = use the mesh's baked materialIndex

    GPUInstance computeGPUInstance(const Mesh& mesh, float elapsedTimeSec) const {
        glm::vec3 animatedPosition = basePosition;
        animatedPosition.y += bobAmplitude * sin(elapsedTimeSec * bobSpeed * 2.0f * glm::pi<float>());

        glm::mat4 transform(1.0f);
        transform = glm::translate(transform, animatedPosition);
        transform = glm::rotate(transform, glm::radians(spinDegPerSec * elapsedTimeSec), glm::vec3(0.0f, 1.0f, 0.0f));
        transform = glm::rotate(transform, glm::radians(baseRotationDeg.z), glm::vec3(0.0f, 0.0f, 1.0f));
        transform = glm::rotate(transform, glm::radians(baseRotationDeg.y), glm::vec3(0.0f, 1.0f, 0.0f));
        transform = glm::rotate(transform, glm::radians(baseRotationDeg.x), glm::vec3(1.0f, 0.0f, 0.0f));
        transform = glm::scale(transform, glm::vec3(scale));

        GPUInstance inst;
        inst.transform = transform;
        inst.invTransform = glm::inverse(transform);
        inst.blasRootIndex = mesh.blasRootIndex;
        inst.materialOverride = materialOverride;

        // A rotated box's world AABB is the union of its 8 transformed corners --
        // transforming localAabbMin/Max directly would give a too-small box.
        glm::vec3 worldMin(std::numeric_limits<float>::infinity());
        glm::vec3 worldMax(-std::numeric_limits<float>::infinity());
        for (int i = 0; i < 8; i++) {
            glm::vec3 corner(
                (i & 1) ? mesh.localAabbMax.x : mesh.localAabbMin.x,
                (i & 2) ? mesh.localAabbMax.y : mesh.localAabbMin.y,
                (i & 4) ? mesh.localAabbMax.z : mesh.localAabbMin.z
            );
            glm::vec3 worldCorner = glm::vec3(transform * glm::vec4(corner, 1.0f));
            worldMin = glm::min(worldMin, worldCorner);
            worldMax = glm::max(worldMax, worldCorner);
        }
        inst.worldAabbMin = worldMin;
        inst.worldAabbMax = worldMax;

        return inst;
    }
};
