#ifndef CAUSTICS_GLSL
#define CAUSTICS_GLSL

struct GPUPhoton {
    vec4 position;
    vec4 direction;
    vec4 energy;
};

// Bindings for reading the photon map (Must match your descriptor set layout)
layout(std430, set = 0, binding = 10) readonly buffer PhotonBuffer { GPUPhoton photons[]; };
layout(std430, set = 0, binding = 11) readonly buffer PhotonCounter { uint globalPhotonCount; };
layout(std430, set = 0, binding = 12) readonly buffer GridHeadBuffer { int gridHeads[]; };
layout(std430, set = 0, binding = 13) readonly buffer PhotonNextBuffer { int photonNexts[]; };

ivec3 getGridCoords(vec3 pos, vec3 gridMin, vec3 gridMax, int gridRes) {
    vec3 norm = (pos - gridMin) / (gridMax - gridMin);
    return ivec3(clamp(norm * vec3(gridRes), vec3(0.0), vec3(gridRes - 1)));
}

int getGridIndex(ivec3 coords, int gridRes) {
    return coords.x + coords.y * gridRes + coords.z * gridRes * gridRes;
}

vec3 gatherPhotons(vec3 hitPos, vec3 hitNormal, vec3 gridMin, vec3 gridMax, int gridRes, int totalEmittedPhotons, float gatherRadius, float causticIntensity) {
    vec3 gatheredEnergy = vec3(0.0);
    float radiusSquared = gatherRadius * gatherRadius;
    ivec3 centerCoords = getGridCoords(hitPos, gridMin, gridMax, gridRes);
    int maxListDepth = 800 + min(0, totalEmittedPhotons);
    
    for (int i = 0; i < 125; i++) {
        int xOff = (i % 5) - 2;
        int yOff = ((i / 5) % 5) - 2;
        int zOff = (i / 25) - 2;
        
        ivec3 searchCoords = centerCoords + ivec3(xOff, yOff, zOff);
        
        if (searchCoords.x >= 0 && searchCoords.x < gridRes &&
            searchCoords.y >= 0 && searchCoords.y < gridRes &&
            searchCoords.z >= 0 && searchCoords.z < gridRes) {
            
            int gridIdx = getGridIndex(searchCoords, gridRes);
            int currentPhotonIdx = gridHeads[gridIdx];
            
            for (int step = 0; step < maxListDepth; step++) {
                if (currentPhotonIdx == -1) break;
                
                vec3 photonPos = photons[currentPhotonIdx].position.xyz;
                vec3 distVec = photonPos - hitPos;
                float d2 = dot(distVec, distVec);
                
                if (d2 < radiusSquared) {
                    if (dot(photons[currentPhotonIdx].direction.xyz, hitNormal) < 0.0) {
                        float weight = 1.0 - (sqrt(d2) / gatherRadius);
                        gatheredEnergy += photons[currentPhotonIdx].energy.xyz * weight;
                    }
                }
                
                currentPhotonIdx = photonNexts[currentPhotonIdx];
            }
        }
    }
    
    float area = 3.14159265359 * radiusSquared;
    return (gatheredEnergy / area) * causticIntensity; 
}
#endif