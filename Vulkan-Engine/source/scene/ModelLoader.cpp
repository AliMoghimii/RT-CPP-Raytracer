#include "scene/ModelLoader.hpp"
#include "math/MathUtils.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <limits>

using namespace std;

// ---- SAH BVH helpers (file-local) ----

struct AABB {
    glm::vec3 min{ std::numeric_limits<float>::infinity() };
    glm::vec3 max{ -std::numeric_limits<float>::infinity() };
};

struct BinInfo {
    AABB bounds;
    int  count = 0;
};

static glm::vec3 triCentroid(const GPUTriangle& t) {
    return (t.v0 + t.v1 + t.v2) / 3.0f;
}

static AABB triAABB(const GPUTriangle& t) {
    return { glm::min(glm::min(t.v0, t.v1), t.v2),
             glm::max(glm::max(t.v0, t.v1), t.v2) };
}

static AABB unionAABB(const AABB& a, const AABB& b) {
    return { glm::min(a.min, b.min), glm::max(a.max, b.max) };
}

static float surfaceArea(const AABB& box) {
    glm::vec3 e = box.max - box.min;
    return 2.0f * (e.x * e.y + e.y * e.z + e.z * e.x);
}

static float surfaceArea(const GPUBVHNode& node) {
    glm::vec3 e = node.aabbMax - node.aabbMin;
    return 2.0f * (e.x * e.y + e.y * e.z + e.z * e.x);
}

void ModelLoader::load(const string& filename, vector<GPUTriangle>& sceneTriangles, vector<GPUBVHNode>& bvhNodes, int materialIndex, const glm::vec3& position, const glm::vec3& rotation, float scale) {
    string extension = "";
    size_t dotPos = filename.find_last_of('.');

    if (dotPos != string::npos) {
        extension = filename.substr(dotPos + 1);
    }

    if (extension == "obj" || extension == "OBJ") {
        loadOBJ(filename, sceneTriangles, materialIndex, position, rotation, scale);

        cout << "ModelLoader: Building BVH...\n";
        buildBVH(sceneTriangles, bvhNodes);
        cout << "ModelLoader: BVH Built successfully.\n";
    }
    else {
        cout << "Error ModelLoader: Unsupported file format '." << extension << "'\n";
    }
}

void ModelLoader::loadOBJ(const string& filename, vector<GPUTriangle>& sceneTriangles, int materialIndex, const glm::vec3& position, const glm::vec3& rotation, float scale) {
    ifstream file(filename);

    if (!file.is_open()) {
        cout << "Error ModelLoader: Could not open file.\n";
        return;
    }

    vector<glm::vec3> vertices;
    vector<glm::vec3> normals;
    string line;
    int facesCount = 0;

    while (getline(file, line)) {
        stringstream ss(line);
        string prefix;
        ss >> prefix;

        if (prefix == "v") {
            float x;
            float y;
            float z;
            ss >> x >> y >> z;

            glm::vec3 v(x, y, z);
            v = v * scale;
            v = MathUtils::rotateVec(v, rotation);
            v = v + position;

            vertices.push_back(v);
        }
        else if (prefix == "vn") {
            float nx;
            float ny;
            float nz;
            ss >> nx >> ny >> nz;

            glm::vec3 n(nx, ny, nz);
            n = MathUtils::rotateVec(n, rotation);
            n = glm::normalize(n);

            normals.push_back(n);
        }
        else if (prefix == "f") {
            string token;
            vector<int> vIndices;
            vector<int> vnIndices;

            while (ss >> token) {
                int v_idx = -1;
                int vt_idx = -1;
                int vn_idx = -1;

                stringstream token_ss(token);
                string item;
                int index = 0;

                while (getline(token_ss, item, '/')) {
                    if (!item.empty()) {
                        if (index == 0) {
                            v_idx = stoi(item) - 1;
                        }
                        else if (index == 1) {
                            vt_idx = stoi(item) - 1;
                        }
                        else if (index == 2) {
                            vn_idx = stoi(item) - 1;
                        }
                    }
                    index++;
                }

                vIndices.push_back(v_idx);
                if (vn_idx != -1) {
                    vnIndices.push_back(vn_idx);
                }
            }

            if (vIndices.size() >= 3) {
                for (size_t i = 1; i < vIndices.size() - 1; ++i) {
                    GPUTriangle tri;

                    tri.uv0x = 0.0f;
                    tri.uv0y = 0.0f;
                    tri.uv1x = 0.0f;
                    tri.uv1y = 0.0f;
                    tri.uv2x = 0.0f;
                    tri.uv2y = 0.0f;
                    tri.p7 = 0.0f;
                    tri.p8 = 0.0f;

                    tri.v0 = vertices[vIndices[0]];
                    tri.v1 = vertices[vIndices[i]];
                    tri.v2 = vertices[vIndices[i + 1]];
                    tri.materialIndex = materialIndex;

                    if (vnIndices.size() == vIndices.size()) {
                        tri.n0 = normals[vnIndices[0]];
                        tri.n1 = normals[vnIndices[i]];
                        tri.n2 = normals[vnIndices[i + 1]];
                        tri.isSmooth = 1;
                    }
                    else {
                        glm::vec3 edge1 = tri.v1 - tri.v0;
                        glm::vec3 edge2 = tri.v2 - tri.v0;
                        glm::vec3 flatNormal = glm::normalize(glm::cross(edge1, edge2));

                        tri.n0 = flatNormal;
                        tri.n1 = flatNormal;
                        tri.n2 = flatNormal;
                        tri.isSmooth = 0;
                    }

                    sceneTriangles.push_back(tri);
                    facesCount++;
                }
            }
        }
    }
    cout << "ModelLoader: Loaded " << filename << " with " << vertices.size() << " vertices and " << facesCount << " faces.\n"; 
}

void ModelLoader::updateNodeBounds(int nodeIdx, vector<GPUBVHNode>& bvhNodes, const vector<GPUTriangle>& triangles) {
    GPUBVHNode& node = bvhNodes[nodeIdx];
    node.aabbMin = glm::vec3(1e30f);
    node.aabbMax = glm::vec3(-1e30f);

    for (int i = 0; i < node.triCount; i++) {
        const GPUTriangle& tri = triangles[node.leftFirst + i];

        glm::vec3 minV = glm::min(glm::min(tri.v0, tri.v1), tri.v2);
        glm::vec3 maxV = glm::max(glm::max(tri.v0, tri.v1), tri.v2);

        node.aabbMin = glm::min(node.aabbMin, minV);
        node.aabbMax = glm::max(node.aabbMax, maxV);
    }
}

void ModelLoader::subdivide(int nodeIdx, vector<GPUBVHNode>& bvhNodes, vector<GPUTriangle>& triangles, int& nodesUsed) {
    GPUBVHNode& node = bvhNodes[nodeIdx];

    if (node.triCount <= 2) {
        return;
    }

    glm::vec3 extent = node.aabbMax - node.aabbMin;
    int axis = 0;

    if (extent.y > extent.x) {
        axis = 1;
    }
    if (extent.z > extent[axis]) {
        axis = 2;
    }

    float splitPos = node.aabbMin[axis] + extent[axis] * 0.5f;

    int i = node.leftFirst;
    int j = i + node.triCount - 1;

    while (i <= j) {
        glm::vec3 centroid = (triangles[i].v0 + triangles[i].v1 + triangles[i].v2) / 3.0f;
        if (centroid[axis] < splitPos) {
            i++;
        }
        else {
            swap(triangles[i], triangles[j]);
            j--;
        }
    }

    int leftCount = i - node.leftFirst;

    if (leftCount == 0 || leftCount == node.triCount) {
        return;
    }

    int leftChildIdx = nodesUsed;
    nodesUsed++;

    int rightChildIdx = nodesUsed;
    nodesUsed++;

    bvhNodes[leftChildIdx].leftFirst = node.leftFirst;
    bvhNodes[leftChildIdx].triCount = leftCount;
    updateNodeBounds(leftChildIdx, bvhNodes, triangles);

    bvhNodes[rightChildIdx].leftFirst = i;
    bvhNodes[rightChildIdx].triCount = node.triCount - leftCount;
    updateNodeBounds(rightChildIdx, bvhNodes, triangles);

    node.leftFirst = leftChildIdx;
    node.triCount = 0;

    subdivide(leftChildIdx, bvhNodes, triangles, nodesUsed);
    subdivide(rightChildIdx, bvhNodes, triangles, nodesUsed);
}

// Surface Area Heuristic (SAH) BVH construction.
//
// The SAH estimates the expected ray-traversal cost for a given split as:
//   cost(split) = 1 + SA(left)/SA(parent) * N_left + SA(right)/SA(parent) * N_right
// where SA is surface area (proportional to the probability a random ray hits that AABB)
// and N is the number of triangles in the child.  The +1 accounts for the AABB test cost.
//
// Binned SAH: instead of testing every triangle centroid as a split plane candidate
// (O(N^2)), we divide the centroid range into NUM_BINS equal-width bins and evaluate
// the SAH cost at each of the NUM_BINS-1 bin boundaries (O(N*B) time).
// We then pick the split axis + position with the lowest cost and recurse.
// If no split improves over keeping all triangles in one leaf, we stop.
void ModelLoader::subdivideSAH(int nodeIdx, vector<GPUBVHNode>& bvhNodes,
                               vector<GPUTriangle>& triangles, int& nodesUsed)
{
    GPUBVHNode& node = bvhNodes[nodeIdx];
    if (node.triCount <= 4) return;  // leaf threshold: small nodes are cheaper to test directly

    const int NUM_BINS = 16;
    float bestCost  = std::numeric_limits<float>::infinity();
    int   bestAxis  = -1;
    float bestSplit = 0.0f;

    // Try splitting along each of the 3 axes; keep the cheapest overall.
    for (int axis = 0; axis < 3; axis++) {
        float axisMin =  std::numeric_limits<float>::infinity();
        float axisMax = -std::numeric_limits<float>::infinity();

        // Find the range of centroids along this axis to size the bins.
        for (int i = 0; i < node.triCount; i++) {
            float c = triCentroid(triangles[node.leftFirst + i])[axis];
            axisMin = std::min(axisMin, c);
            axisMax = std::max(axisMax, c);
        }
        if (axisMax - axisMin < 1e-6f) continue;  // all centroids coplanar on this axis: skip

        float scale = float(NUM_BINS) / (axisMax - axisMin);

        // Scatter triangles into bins based on centroid position.
        BinInfo bins[NUM_BINS] = {};
        for (int i = 0; i < node.triCount; i++) {
            const GPUTriangle& tri = triangles[node.leftFirst + i];
            int b = std::min(NUM_BINS - 1, int((triCentroid(tri)[axis] - axisMin) * scale));
            bins[b].count++;
            bins[b].bounds = unionAABB(bins[b].bounds, triAABB(tri));
        }

        // Prefix sums: lCounts[i]/lAABB[i] = total count/AABB for bins [0..i].
        // Suffix sums: rCounts[i]/rAABB[i] = total count/AABB for bins [i..N-1].
        // These allow O(1) SAH evaluation for each candidate split plane.
        int  lCounts[NUM_BINS], rCounts[NUM_BINS];
        AABB lAABB[NUM_BINS],   rAABB[NUM_BINS];

        int  lSum = 0; AABB lAcc;
        for (int i = 0; i < NUM_BINS; i++) {
            lSum += bins[i].count; lCounts[i] = lSum;
            lAcc = unionAABB(lAcc, bins[i].bounds); lAABB[i] = lAcc;
        }

        int  rSum = 0; AABB rAcc;
        for (int i = NUM_BINS - 1; i >= 0; i--) {
            rSum += bins[i].count; rCounts[i] = rSum;
            rAcc = unionAABB(rAcc, bins[i].bounds); rAABB[i] = rAcc;
        }

        float parentSA = surfaceArea(node);
        // Evaluate SAH cost at each bin boundary (split s = "bins 0..s go left, s+1..N-1 go right").
        for (int s = 0; s < NUM_BINS - 1; s++) {
            if (lCounts[s] == 0 || rCounts[s + 1] == 0) continue;
            float cost = 1.0f
                + (surfaceArea(lAABB[s])     / parentSA) * float(lCounts[s])
                + (surfaceArea(rAABB[s + 1]) / parentSA) * float(rCounts[s + 1]);
            if (cost < bestCost) {
                bestCost  = cost;
                bestAxis  = axis;
                bestSplit = axisMin + float(s + 1) / scale;
            }
        }
    }

    // If no split reduces cost below the leaf cost (N triangles directly), leave as a leaf.
    if (bestAxis == -1 || bestCost >= float(node.triCount)) return;

    // Partition triangles in-place around the split plane (Dutch flag / lomuto-style).
    int i = node.leftFirst, j = i + node.triCount - 1;
    while (i <= j) {
        if (triCentroid(triangles[i])[bestAxis] < bestSplit) i++;
        else std::swap(triangles[i], triangles[j--]);
    }

    int leftCount = i - node.leftFirst;
    if (leftCount == 0 || leftCount == node.triCount) return;  // degenerate: all on one side

    int leftChild  = nodesUsed++;
    int rightChild = nodesUsed++;

    bvhNodes[leftChild].leftFirst  = node.leftFirst;
    bvhNodes[leftChild].triCount   = leftCount;
    updateNodeBounds(leftChild, bvhNodes, triangles);

    bvhNodes[rightChild].leftFirst = i;
    bvhNodes[rightChild].triCount  = node.triCount - leftCount;
    updateNodeBounds(rightChild, bvhNodes, triangles);

    // Convert this node from leaf to internal: leftFirst now points to the left child.
    // triCount = 0 signals "internal node" to the GPU traversal shader.
    node.leftFirst = leftChild;
    node.triCount  = 0;

    subdivideSAH(leftChild,  bvhNodes, triangles, nodesUsed);
    subdivideSAH(rightChild, bvhNodes, triangles, nodesUsed);
}

void ModelLoader::buildBVH(vector<GPUTriangle>& triangles, vector<GPUBVHNode>& bvhNodes) {
    if (triangles.empty()) {
        return;
    }

    bvhNodes.resize(triangles.size() * 2);

    GPUBVHNode& root = bvhNodes[0];
    root.leftFirst = 0;
    root.triCount = static_cast<int>(triangles.size());

    updateNodeBounds(0, bvhNodes, triangles);

    int nodesUsed = 1;
    subdivideSAH(0, bvhNodes, triangles, nodesUsed);

    bvhNodes.resize(nodesUsed);
}