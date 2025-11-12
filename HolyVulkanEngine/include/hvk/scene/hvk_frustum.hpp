/**
 * @file hvk_frustum.hpp
 * @brief View frustum culling utilities
 * @author Holy Vulkan Engine
 * @date 2025
 * Provides frustum extraction and AABB/sphere culling tests.
 */

#ifndef HVK_FRUSTUM_HPP
#define HVK_FRUSTUM_HPP

#include <hvk/resources/hvk_mesh.h>  // For AABB
#include <glm/glm.hpp>
#include <array>

namespace hvk {

/**
 * Plane - 3D plane defined by normal and distance
 *
 * Plane equation: dot(normal, point) + distance = 0
 * - Points on the plane satisfy the equation
 * - Positive distance = point in front of plane (outside halfspace)
 * - Negative distance = point behind plane (inside halfspace)
 */
struct Plane {
    glm::vec3 normal{0.0f, 1.0f, 0.0f};  // Plane normal (normalized)
    float distance = 0.0f;                // Distance from origin

    Plane() = default;

    Plane(const glm::vec3& n, float d)
        : normal(glm::normalize(n)), distance(d) {}

    /**
     * Construct plane from normal and point on plane
     */
    Plane(const glm::vec3& n, const glm::vec3& point)
        : normal(glm::normalize(n))
        , distance(-glm::dot(normal, point)) {}

    /**
     * Signed distance from point to plane
     * - Positive: point in front of plane
     * - Negative: point behind plane
     * - Zero: point on plane
     */
    float signedDistance(const glm::vec3& point) const {
        return glm::dot(normal, point) + distance;
    }

    /**
     * Normalize the plane equation
     */
    void normalize() {
        float len = glm::length(normal);
        normal /= len;
        distance /= len;
    }
};

/**
 * Frustum - View frustum defined by 6 planes
 *
 * Planes are ordered: Left, Right, Bottom, Top, Near, Far
 * Normals point INWARD (toward frustum interior)
 *
 * Usage:
 *   Frustum frustum = Frustum::fromViewProjection(viewProj);
 *   if (frustum.testAABB(bounds) != Frustum::Outside) {
 *       // Render the object
 *   }
 */
class Frustum {
public:
    enum TestResult {
        Outside,    // Object is completely outside frustum (cull it)
        Intersect,  // Object intersects frustum (render it)
        Inside      // Object is completely inside frustum (render it)
    };

    enum PlaneIndex {
        Left = 0,
        Right = 1,
        Bottom = 2,
        Top = 3,
        Near = 4,
        Far = 5
    };

    Frustum() = default;

    /**
     * Extract frustum planes from view-projection matrix
     *
     * Uses Gribb-Hartmann method (fast, widely used)
     * Reference: http://www8.cs.umu.se/kurser/5DV051/HT12/lab/plane_extraction.pdf
     *
     * @param viewProj Combined view-projection matrix
     */
    static Frustum fromViewProjection(const glm::mat4& viewProj) {
        Frustum frustum;

        // Extract planes from matrix rows
        // Note: GLM matrices are column-major, so we access [col][row]

        // Left plane: row4 + row1
        frustum.planes_[Left] = Plane(
            glm::vec3(viewProj[0][3] + viewProj[0][0],
                     viewProj[1][3] + viewProj[1][0],
                     viewProj[2][3] + viewProj[2][0]),
            viewProj[3][3] + viewProj[3][0]
        );

        // Right plane: row4 - row1
        frustum.planes_[Right] = Plane(
            glm::vec3(viewProj[0][3] - viewProj[0][0],
                     viewProj[1][3] - viewProj[1][0],
                     viewProj[2][3] - viewProj[2][0]),
            viewProj[3][3] - viewProj[3][0]
        );

        // Bottom plane: row4 + row2
        frustum.planes_[Bottom] = Plane(
            glm::vec3(viewProj[0][3] + viewProj[0][1],
                     viewProj[1][3] + viewProj[1][1],
                     viewProj[2][3] + viewProj[2][1]),
            viewProj[3][3] + viewProj[3][1]
        );

        // Top plane: row4 - row2
        frustum.planes_[Top] = Plane(
            glm::vec3(viewProj[0][3] - viewProj[0][1],
                     viewProj[1][3] - viewProj[1][1],
                     viewProj[2][3] - viewProj[2][1]),
            viewProj[3][3] - viewProj[3][1]
        );

        // Near plane: row4 + row3
        frustum.planes_[Near] = Plane(
            glm::vec3(viewProj[0][3] + viewProj[0][2],
                     viewProj[1][3] + viewProj[1][2],
                     viewProj[2][3] + viewProj[2][2]),
            viewProj[3][3] + viewProj[3][2]
        );

        // Far plane: row4 - row3
        frustum.planes_[Far] = Plane(
            glm::vec3(viewProj[0][3] - viewProj[0][2],
                     viewProj[1][3] - viewProj[1][2],
                     viewProj[2][3] - viewProj[2][2]),
            viewProj[3][3] - viewProj[3][2]
        );

        // Normalize all planes
        for (auto& plane : frustum.planes_) {
            plane.normalize();
        }

        return frustum;
    }

    /**
     * Test AABB against frustum
     *
     * Uses separating axis theorem with plane normals as axes.
     * Returns Outside if AABB is completely outside any plane.
     *
     * @param aabb Axis-aligned bounding box to test
     * @return TestResult indicating visibility
     */
    TestResult testAABB(const AABB& aabb) const {
        if (!aabb.isValid()) {
            return Outside;  // Invalid AABB is considered outside
        }

        TestResult result = Inside;

        for (const auto& plane : planes_) {
            // Find the positive vertex (farthest along plane normal)
            glm::vec3 pVertex = aabb.min;
            if (plane.normal.x >= 0) pVertex.x = aabb.max.x;
            if (plane.normal.y >= 0) pVertex.y = aabb.max.y;
            if (plane.normal.z >= 0) pVertex.z = aabb.max.z;

            // Find the negative vertex (nearest along plane normal)
            glm::vec3 nVertex = aabb.max;
            if (plane.normal.x >= 0) nVertex.x = aabb.min.x;
            if (plane.normal.y >= 0) nVertex.y = aabb.min.y;
            if (plane.normal.z >= 0) nVertex.z = aabb.min.z;

            // If positive vertex is outside, AABB is completely outside
            if (plane.signedDistance(pVertex) < 0) [[unlikely]] {
                return Outside;
            }

            // If negative vertex is outside, AABB intersects plane
            if (plane.signedDistance(nVertex) < 0) [[unlikely]] {
                result = Intersect;
            }
        }

        return result;
    }

    /**
     * Test AABB against frustum (simple binary test)
     *
     * @param aabb Axis-aligned bounding box to test
     * @return true if AABB is visible (inside or intersecting), false if completely outside
     */
    bool testAABBSimple(const AABB& aabb) const {
        return testAABB(aabb) != Outside;
    }

    /**
     * Test sphere against frustum
     *
     * @param center Sphere center
     * @param radius Sphere radius
     * @return TestResult indicating visibility
     */
    TestResult testSphere(const glm::vec3& center, float radius) const {
        TestResult result = Inside;

        for (const auto& plane : planes_) {
            float dist = plane.signedDistance(center);

            // Sphere completely outside
            if (dist < -radius) {
                return Outside;
            }

            // Sphere intersects plane
            if (dist < radius) {
                result = Intersect;
            }
        }

        return result;
    }

    /**
     * Test point against frustum
     *
     * @param point Point to test
     * @return true if point is inside frustum
     */
    [[nodiscard]] inline bool testPoint(const glm::vec3& point) const noexcept {
        for (const auto& plane : planes_) {
            if (plane.signedDistance(point) < 0) [[unlikely]] {
                return false;
            }
        }
        return true;
    }

    /**
     * Get a specific frustum plane
     */
    const Plane& plane(PlaneIndex index) const {
        return planes_[index];
    }

private:
    std::array<Plane, 6> planes_;
};

} // namespace hvk

#endif // HVK_FRUSTUM_HPP
