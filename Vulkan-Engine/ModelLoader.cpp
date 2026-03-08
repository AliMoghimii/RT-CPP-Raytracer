#include "ModelLoader.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cmath>

using namespace std;

glm::vec3 ModelLoader::rotateVec(glm::vec3 v, glm::vec3 rot) {
    float radX = rot.x * 3.14159265359f / 180.0f;
    float radY = rot.y * 3.14159265359f / 180.0f;
    float radZ = rot.z * 3.14159265359f / 180.0f;

    glm::vec3 rx(v.x, v.y * cos(radX) - v.z * sin(radX), v.y * sin(radX) + v.z * cos(radX));
    glm::vec3 ry(rx.x * cos(radY) + rx.z * sin(radY), rx.y, -rx.x * sin(radY) + rx.z * cos(radY));
    glm::vec3 rz(ry.x * cos(radZ) - ry.y * sin(radZ), ry.x * sin(radZ) + ry.y * cos(radZ), ry.z);

    return rz;
}

void ModelLoader::load(const string& filename, vector<GPUTriangle>& sceneTriangles, int materialIndex, const glm::vec3& position, const glm::vec3& rotation, float scale)
{
    string extension = "";
    size_t dotPos = filename.find_last_of('.');
    if (dotPos != string::npos) {
        extension = filename.substr(dotPos + 1);
    }

    if (extension == "obj" || extension == "OBJ") {
        loadOBJ(filename, sceneTriangles, materialIndex, position, rotation, scale);
    }
    else {
        cout << "Error: Unsupported file format '." << extension << "'\n";
    }
}

void ModelLoader::loadOBJ(const string& filename, vector<GPUTriangle>& sceneTriangles, int materialIndex, const glm::vec3& position, const glm::vec3& rotation, float scale)
{
    ifstream file(filename);
    if (!file.is_open())
    {
        cout << "Error: Could not open " << filename << "\n";
        return;
    }

    vector<glm::vec3> vertices;
    vector<glm::vec3> normals;
    string line;
    int facesCount = 0;

    while (getline(file, line))
    {
        stringstream ss(line);
        string prefix;
        ss >> prefix;

        if (prefix == "v")
        {
            float x, y, z;
            ss >> x >> y >> z;
            glm::vec3 v(x, y, z);

            v = v * scale;
            v = rotateVec(v, rotation);
            v = v + position;

            vertices.push_back(v);
        }
        else if (prefix == "vn")
        {
            float nx, ny, nz;
            ss >> nx >> ny >> nz;
            glm::vec3 n(nx, ny, nz);

            n = rotateVec(n, rotation);
            n = glm::normalize(n);
            normals.push_back(n);
        }
        else if (prefix == "f")
        {
            string token;
            vector<int> vIndices;
            vector<int> vnIndices;

            while (ss >> token)
            {
                int v_idx = -1, vt_idx = -1, vn_idx = -1;
                stringstream token_ss(token);
                string item;
                int index = 0;

                while (getline(token_ss, item, '/'))
                {
                    if (!item.empty())
                    {
                        if (index == 0) v_idx = stoi(item) - 1;
                        else if (index == 1) vt_idx = stoi(item) - 1;
                        else if (index == 2) vn_idx = stoi(item) - 1;
                    }
                    index++;
                }

                vIndices.push_back(v_idx);
                if (vn_idx != -1) vnIndices.push_back(vn_idx);
            }

            if (vIndices.size() >= 3)
            {
                for (size_t i = 1; i < vIndices.size() - 1; ++i)
                {
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

                    if (vnIndices.size() == vIndices.size())
                    {
                        tri.n0 = normals[vnIndices[0]];
                        tri.n1 = normals[vnIndices[i]];
                        tri.n2 = normals[vnIndices[i + 1]];
                        tri.isSmooth = 1;
                    }
                    else
                    {
                        glm::vec3 edge1 = tri.v1 - tri.v0;
                        glm::vec3 edge2 = tri.v2 - tri.v0;
                        glm::vec3 flatNormal = glm::normalize(glm::cross(edge1, edge2));

                        tri.n0 = flatNormal; tri.n1 = flatNormal; tri.n2 = flatNormal;
                        tri.isSmooth = 0;
                    }

                    sceneTriangles.push_back(tri);
                    facesCount++;
                }
            }
        }
    }
    cout << "Loaded " << filename << " with " << vertices.size() << " vertices and " << facesCount << " faces.\n";
}