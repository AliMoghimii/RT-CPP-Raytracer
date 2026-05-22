#include "scene/Primitive.hpp"
#include "math/MathUtils.hpp"
#include <glm/ext/scalar_constants.hpp>
#include <cmath>

void tessellateSphere(
	const GPUSphere& sphere,
	int materialIndex,
	std::vector<GPUTriangle>& outTriangles,
	int segmentsU,
	int segmentsV
)
{
	std::vector<glm::vec3> vertices;
	std::vector<glm::vec3> normals;

	for (int v = 0; v <= segmentsV; ++v) {
		float phi = glm::pi<float>() * v / segmentsV;
		for (int u = 0; u <= segmentsU; ++u) {
			float theta = 2.0f * glm::pi<float>() * u / segmentsU;
			glm::vec3 normal(
				sin(phi) * cos(theta),
				cos(phi),
				sin(phi) * sin(theta)
			);
			glm::vec3 vertex = sphere.center + normal * sphere.radius;
			vertices.push_back(vertex);
			normals.push_back(normal);
		}
	}

	auto idx = [&](int u, int v) {
		return v * (segmentsU + 1) + u;
	};

	for (int v = 0; v < segmentsV; ++v) {
		for (int u = 0; u < segmentsU; ++u) {
			// Two triangles per quad
			GPUTriangle t1{};
			t1.v0 = vertices[idx(u, v)];		t1.n0 = normals[idx(u, v)];
			t1.v1 = vertices[idx(u + 1, v)];	t1.n1 = normals[idx(u + 1, v)];
			t1.v2 = vertices[idx(u, v + 1)];	t1.n2 = normals[idx(u, v + 1)];
			t1.isSmooth = 1;
			t1.materialIndex = materialIndex;
			outTriangles.push_back(t1);

			GPUTriangle t2{};
			t2.v0 = vertices[idx(u + 1, v)];		t2.n0 = normals[idx(u + 1, v)];
			t2.v1 = vertices[idx(u + 1, v + 1)];	t2.n1 = normals[idx(u + 1, v + 1)];
			t2.v2 = vertices[idx(u, v + 1)];		t2.n2 = normals[idx(u, v + 1)];
			t2.isSmooth = 1;
			t2.materialIndex = materialIndex;
			outTriangles.push_back(t2);
		}
	}
}

void tessellateCube(
	const GPUCube& cube,
	int materialIndex,
	std::vector<GPUTriangle>& outTriangles
)
{
	glm::vec3 halfExtents = (cube.boundsMax - cube.boundsMin) * 0.5f;
	glm::vec3 center = cube.center + (cube.boundsMin + cube.boundsMax) * 0.5f;


	// 8 corners of the box in local space
	glm::vec3 corners[8];
	for (int i = 0; i < 8; i++)
	{
		corners[i] = glm::vec3(
			(i & 1) ? halfExtents.x : -halfExtents.x,
			(i & 2) ? halfExtents.y : -halfExtents.y,
			(i & 4) ? halfExtents.z : -halfExtents.z
		);

		// Apply rotation (if any)
		corners[i] = MathUtils::rotateVec(corners[i], cube.rotation);
		corners[i] += center; // Translate to world space
	}

	// 6 faces, 2 triangles per face = 12 triangles
	// Face indices (CCW winding from outside)
	static const int faces[6][4] = {
		{0, 2, 3, 1}, // -Z
		{4, 5, 7, 6}, // +Z
		{0, 4, 6, 2}, // -X
		{1, 3, 7, 5}, // +X
		{0, 1, 5, 4}, // -Y
		{2, 6, 7, 3}  // +Y
	};

	static const glm::vec3 faceNormals[6] = {
		{0, 0, -1}, {0, 0, 1}, {-1, 0, 0},
		{1, 0, 0}, {0, -1, 0}, {0, 1, 0}
	};

	for (int f = 0; f < 6; f++)
	{
		glm::vec3 normal = MathUtils::rotateVec(faceNormals[f], cube.rotation);

		GPUTriangle t1{};
		t1.v0 = corners[faces[f][0]];	t1.n0 = normal;
		t1.v1 = corners[faces[f][1]];	t1.n1 = normal;
		t1.v2 = corners[faces[f][2]];	t1.n2 = normal;
		t1.isSmooth = 0;
		t1.materialIndex = materialIndex;
		outTriangles.push_back(t1);

		GPUTriangle t2{};
		t2.v0 = corners[faces[f][0]];	t2.n0 = normal;
		t2.v1 = corners[faces[f][2]];	t2.n1 = normal;
		t2.v2 = corners[faces[f][3]];	t2.n2 = normal;
		t2.isSmooth = 0;
		t2.materialIndex = materialIndex;
		outTriangles.push_back(t2);
	}
}

void tessellateQuad(
	const GPUQuad& quad,
	int materialIndex,
	std::vector<GPUTriangle>& outTriangles
)
{
	glm::vec3 v0 = quad.corner;
	glm::vec3 v1 = quad.corner + quad.edge1;
	glm::vec3 v2 = quad.corner + quad.edge1 + quad.edge2;
	glm::vec3 v3 = quad.corner + quad.edge2;

	GPUTriangle t1{};
	t1.v0 = v0;	t1.n0 = quad.normalVector;
	t1.v1 = v1;	t1.n1 = quad.normalVector;
	t1.v2 = v2;	t1.n2 = quad.normalVector;
	t1.isSmooth = 0;
	t1.materialIndex = materialIndex;
	outTriangles.push_back(t1);

	GPUTriangle t2{};
	t2.v0 = v0;	t2.n0 = quad.normalVector;
	t2.v1 = v2;	t2.n1 = quad.normalVector;
	t2.v2 = v3;	t2.n2 = quad.normalVector;
	t2.isSmooth = 0;
	t2.materialIndex = materialIndex;
	outTriangles.push_back(t2);
}

void tessellatePlane(
	const GPUPlane& plane,
	int materialIndex,
	std::vector<GPUTriangle>& outTriangles,
	float extent
)
{
	// Convert infinite plane to a very large quad
	glm::vec3 n = plane.normalVector;
	glm::vec3 tangent = abs(n.y) < 0.99f
		? glm::normalize(glm::cross(n, glm::vec3(0, 1, 0)))
		: glm::normalize(glm::cross(n, glm::vec3(1, 0, 0)));
	glm::vec3 bitangent = glm::cross(n, tangent);

	GPUQuad bigQuad{};
	bigQuad.corner = plane.center - tangent * extent - bitangent * extent;
	bigQuad.edge1 = tangent * extent * 2.0f;
	bigQuad.edge2 = bitangent * extent * 2.0f;
	bigQuad.normalVector = n;
	bigQuad.materialIndex = materialIndex;

	tessellateQuad(bigQuad, materialIndex, outTriangles);
}