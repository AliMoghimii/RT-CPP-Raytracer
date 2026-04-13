#include "ModelLoader.hpp"
#include "MathUtils.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cmath>
#include <algorithm>

using namespace std;

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

                    tri.p1 = 0.0f;
                    tri.p2 = 0.0f;
                    tri.p3 = 0.0f;
                    tri.p4 = 0.0f;
                    tri.p5 = 0.0f;
                    tri.p6 = 0.0f;
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
    subdivide(0, bvhNodes, triangles, nodesUsed);

    bvhNodes.resize(nodesUsed);
}