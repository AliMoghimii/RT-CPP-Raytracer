#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace MathUtils {
    inline glm::vec3 rotateVec(const glm::vec3& point, const glm::vec3& rotInDegrees) {
        glm::vec3 rads = glm::radians(rotInDegrees);

        glm::mat4 matrix = glm::mat4(1.0f);

        matrix = glm::rotate(matrix, rads.z, glm::vec3(0.0f, 0.0f, 1.0f));
        matrix = glm::rotate(matrix, rads.y, glm::vec3(0.0f, 1.0f, 0.0f));
        matrix = glm::rotate(matrix, rads.x, glm::vec3(1.0f, 0.0f, 0.0f));

        glm::vec4 rotatedPoint = matrix * glm::vec4(point, 1.0f);
        return glm::vec3(rotatedPoint);
    }
}