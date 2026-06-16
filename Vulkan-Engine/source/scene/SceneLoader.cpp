#include "scene/SceneLoader.hpp"
#include "scene/GenericScene.hpp"
#include "scene/ImageLoader.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <stdexcept>
#include <unordered_map>

#include <iostream>

using json = nlohmann::json;

namespace {

glm::vec3 toVec3(const json& j, const glm::vec3& def) {
    if (!j.is_array() || j.size() < 3) return def;
    return glm::vec3(j[0].get<float>(), j[1].get<float>(), j[2].get<float>());
}

glm::vec3 vec3Field(const json& obj, const char* key, const glm::vec3& def) {
    if (obj.contains(key)) return toVec3(obj[key], def);
    return def;
}

template <typename T>
T getOr(const json& obj, const char* key, T def) {
    if (obj.contains(key) && !obj[key].is_null()) return obj[key].get<T>();
    return def;
}

int resolveTextureIndex(const json& materialJson, const char* key,
                         const std::unordered_map<std::string, int>& textureIndex) {
    if (!materialJson.contains(key) || materialJson[key].is_null()) return -1;
    std::string name = materialJson[key].get<std::string>();
    auto it = textureIndex.find(name);
    if (it == textureIndex.end())
        throw std::runtime_error("SceneLoader: unknown texture reference '" + name + "'");
    return it->second;
}

int resolveMaterialIndex(const json& obj,
                          const std::unordered_map<std::string, int>& materialIndex) {
    std::string name = obj.at("material").get<std::string>();
    auto it = materialIndex.find(name);
    if (it == materialIndex.end())
        throw std::runtime_error("SceneLoader: unknown material reference '" + name + "'");
    return it->second;
}

} // namespace

std::unique_ptr<Scene> SceneLoader::loadFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open())
        throw std::runtime_error("SceneLoader: failed to open " + path);

    json root;
    file >> root;

    SceneData data;

    // --- Settings ---
    if (root.contains("settings")) {
        const json& s = root["settings"];
        SceneSettings& settings = data.settings;
        settings.useLegacyPipeline   = getOr(s, "useLegacyPipeline", settings.useLegacyPipeline);
        settings.globalShadingModel  = getOr(s, "globalShadingModel", settings.globalShadingModel);
        settings.enableSoftShadows   = getOr(s, "enableSoftShadows", settings.enableSoftShadows);
        settings.maxDepth             = getOr(s, "maxDepth", settings.maxDepth);
        settings.shadowRays           = getOr(s, "shadowRays", settings.shadowRays);
        settings.enableDOF            = getOr(s, "enableDOF", settings.enableDOF);
        settings.focalDistance        = getOr(s, "focalDistance", settings.focalDistance);
        settings.lensRadius           = getOr(s, "lensRadius", settings.lensRadius);
        settings.enableFog            = getOr(s, "enableFog", settings.enableFog);
        settings.fogColor             = vec3Field(s, "fogColor", settings.fogColor);
        settings.enableSkybox         = getOr(s, "enableSkybox", settings.enableSkybox);
        settings.skyBottomColor       = vec3Field(s, "skyBottomColor", settings.skyBottomColor);
        settings.skyTopColor          = vec3Field(s, "skyTopColor", settings.skyTopColor);
        settings.enableTextures       = getOr(s, "enableTextures", settings.enableTextures);
        settings.primaryRaysPerPixel  = getOr(s, "primaryRaysPerPixel", settings.primaryRaysPerPixel);
        settings.enableCaustics       = getOr(s, "enableCaustics", settings.enableCaustics);
        settings.photonCount          = getOr(s, "photonCount", settings.photonCount);
        settings.causticIntensity     = getOr(s, "causticIntensity", settings.causticIntensity);
        settings.gridMin              = vec3Field(s, "gridMin", settings.gridMin);
        settings.gridMax              = vec3Field(s, "gridMax", settings.gridMax);
        settings.gridResolution       = getOr(s, "gridResolution", settings.gridResolution);
        settings.gatherRadius         = getOr(s, "gatherRadius", settings.gatherRadius);
    }

    // --- Textures (name -> index into data.texturePaths) ---
    std::unordered_map<std::string, int> textureIndex;
    if (root.contains("textures")) {
        for (const auto& texJson : root["textures"]) {
            std::string name = texJson.at("name").get<std::string>();
            std::string texPath = texJson.at("path").get<std::string>();
            int idx = ImageLoader::load(texPath, data.texturePaths);
            textureIndex[name] = idx;
        }
    }

    // --- Materials (name -> index into data.materials) ---
    std::unordered_map<std::string, int> materialIndex;
    if (root.contains("materials")) {
        for (const auto& m : root["materials"]) {
            GPUMaterial mat{};
            mat.color = vec3Field(m, "color", glm::vec3(1.0f));
            mat.ambient = getOr(m, "ambient", 0.0f);
            mat.emission = vec3Field(m, "emission", glm::vec3(0.0f));
            mat.diffuse = getOr(m, "diffuse", 1.0f);
            mat.color2 = vec3Field(m, "color2", glm::vec3(0.0f));
            mat.specular = getOr(m, "specular", 0.0f);

            mat.reflection = getOr(m, "reflection", 0.0f);
            mat.transparency = getOr(m, "transparency", 0.0f);
            mat.ior = getOr(m, "ior", 1.0f);
            mat.shadingModel = data.settings.globalShadingModel;

            mat.patternType = getOr(m, "patternType", 0);
            mat.roughness = getOr(m, "roughness", 0.0f);
            mat.metallic = getOr(m, "metallic", 0.0f);
            mat.castShadows = getOr(m, "castShadows", 1);

            mat.useTexture = getOr(m, "useTexture", 0);
            mat.albedoIndex = resolveTextureIndex(m, "albedo", textureIndex);
            mat.normalMapIndex = resolveTextureIndex(m, "normalMap", textureIndex);
            mat.roughnessIndex = resolveTextureIndex(m, "roughnessMap", textureIndex);

            mat.aoIndex = resolveTextureIndex(m, "ao", textureIndex);
            mat.heightMapIndex = resolveTextureIndex(m, "heightMap", textureIndex);
            mat.proceduralScale = getOr(m, "proceduralScale", 0.0f);
            mat.proceduralWobble = getOr(m, "proceduralWobble", 0.0f);

            mat.bumpStrength = getOr(m, "bumpStrength", 0.0f);
            mat.parallaxScale = getOr(m, "parallaxScale", 0.0f);
            mat.p5 = 0.0f;
            mat.p6 = 0.0f;

            std::string name = m.at("name").get<std::string>();
            materialIndex[name] = (int)data.materials.size();
            data.materials.push_back(mat);
        }
    }

    // --- Lights (final color/radius values, no implicit settings-derived scaling) ---
    if (root.contains("lights")) {
        for (const auto& l : root["lights"]) {
            GPULight light{};

            light.position = vec3Field(l, "position", glm::vec3(0.0f));

            if (data.settings.enableSoftShadows) {
                light.radius = 5.0f;
            }
            else
            {
                light.radius = getOr(l, "radius", 0.0f);
            }

            if (data.settings.globalShadingModel == 1) {
                light.color = vec3Field(l, "color", glm::vec3(1.0f)) * 3.0f;
            }
            else
                light.color = vec3Field(l, "color", glm::vec3(1.0f));

            light.p2 = 0.0f;
            data.lights.push_back(light);
        }
    }

    // --- Planes ---
    if (root.contains("planes")) {
        for (const auto& p : root["planes"]) {
            GPUPlane plane{};
            plane.center = vec3Field(p, "center", glm::vec3(0.0f));
            plane.normalVector = vec3Field(p, "normal", glm::vec3(0.0f, 1.0f, 0.0f));
            plane.materialIndex = resolveMaterialIndex(p, materialIndex);
            data.planes.push_back(plane);
        }
    }

    // --- Quads ---
    if (root.contains("quads")) {
        for (const auto& q : root["quads"]) {
            GPUQuad quad{};
            quad.corner = vec3Field(q, "corner", glm::vec3(0.0f));
            quad.edge1 = vec3Field(q, "edge1", glm::vec3(0.0f));
            quad.edge2 = vec3Field(q, "edge2", glm::vec3(0.0f));
            quad.normalVector = vec3Field(q, "normal", glm::vec3(0.0f, 1.0f, 0.0f));
            quad.materialIndex = resolveMaterialIndex(q, materialIndex);
            data.quads.push_back(quad);
        }
    }

    // --- Cubes ---
    if (root.contains("cubes")) {
        for (const auto& c : root["cubes"]) {
            GPUCube cube{};
            cube.boundsMin = vec3Field(c, "boundsMin", glm::vec3(0.0f));
            cube.boundsMax = vec3Field(c, "boundsMax", glm::vec3(0.0f));
            cube.center = vec3Field(c, "center", glm::vec3(0.0f));
            cube.rotation = vec3Field(c, "rotation", glm::vec3(0.0f));
            cube.materialIndex = resolveMaterialIndex(c, materialIndex);
            data.cubes.push_back(cube);
        }
    }

    // --- Spheres ---
    if (root.contains("spheres")) {
        for (const auto& sp : root["spheres"]) {
            GPUSphere sphere{};
            sphere.center = vec3Field(sp, "center", glm::vec3(0.0f));
            sphere.radius = getOr(sp, "radius", 1.0f);
            sphere.materialIndex = resolveMaterialIndex(sp, materialIndex);
            data.spheres.push_back(sphere);
        }
    }

    if (root.contains("triangles")) {
        for (const auto& t : root["triangles"]) {
            GPUTriangle tri{};
            tri.v0 = vec3Field(t, "v0", glm::vec3(0.0f));
            tri.v1 = vec3Field(t, "v1", glm::vec3(0.0f));
            tri.v2 = vec3Field(t, "v2", glm::vec3(0.0f));

            glm::vec3 n = vec3Field(t, "normal", glm::vec3(0.0f, 1.0f, 0.0f));
            tri.n0 = n;
            tri.n1 = n;
            tri.n2 = n;

            tri.isSmooth = getOr(t, "isSmooth", 0);
            tri.materialIndex = resolveMaterialIndex(t, materialIndex);

            data.triangles.push_back(tri);
        }
    }

    // --- Mesh instances (TLAS/BLAS) ---
    if (root.contains("meshInstances")) {
        std::unordered_map<std::string, int> meshFileIndex;
        for (const auto& mi : root["meshInstances"]) {
            std::string meshFile = mi.at("meshFile").get<std::string>();
            int matIdx = resolveMaterialIndex(mi, materialIndex);

            auto it = meshFileIndex.find(meshFile);
            int meshIdx;
            if (it == meshFileIndex.end()) {
                meshIdx = (int)data.meshFiles.size();
                meshFileIndex[meshFile] = meshIdx;
                data.meshFiles.push_back(meshFile);
                data.meshMaterialIndices.push_back(matIdx);
            } else {
                meshIdx = it->second;
            }

            MeshInstance inst;
            inst.meshIndex = meshIdx;
            inst.basePosition = vec3Field(mi, "basePosition", glm::vec3(0.0f));
            inst.baseRotationDeg = vec3Field(mi, "baseRotationDeg", glm::vec3(0.0f));
            inst.scale = getOr(mi, "scale", 1.0f);
            inst.spinDegPerSec = getOr(mi, "spinDegPerSec", 0.0f);
            inst.bobAmplitude = getOr(mi, "bobAmplitude", 0.0f);
            inst.bobSpeed = getOr(mi, "bobSpeed", 0.0f);
            inst.materialOverride = matIdx;
            data.meshInstances.push_back(inst);
        }
    }

    return std::make_unique<GenericScene>(std::move(data));
}
