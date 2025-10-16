#ifndef HVK_PRIMITIVES_H
#define HVK_PRIMITIVES_H

#include <hvk/resources/hvk_mesh.h>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <vector>
#include <cmath>

namespace hvk {

/**
 * Primitives - Static factory for generating common 3D primitive shapes
 *
 * Usage:
 *   auto [vertices, indices] = Primitives::createCube();
 *   Mesh mesh;
 *   mesh.create(device, uploader, vertices, indices, material, "cube");
 *
 * All primitives:
 * - Have proper normals for lighting
 * - Have UVs for texturing
 * - Have tangents for normal mapping
 * - Are centered at origin
 * - Use right-handed coordinate system (Vulkan conventions)
 */
class Primitives {
public:
    /**
     * Create a unit cube (1x1x1) centered at origin
     *
     * - 24 vertices (unique normals per face)
     * - 36 indices (12 triangles, 2 per face)
     * - UVs: 0-1 range per face
     */
    static std::pair<std::vector<Vertex>, std::vector<uint32_t>> createCube(float size = 1.0f) {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;

        float halfSize = size * 0.5f;

        // Helper to add a quad face
        auto addFace = [&](
            const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& p3,
            const glm::vec3& normal, const glm::vec3& tangent
        ) {
            uint32_t baseIdx = static_cast<uint32_t>(vertices.size());

            vertices.push_back({});
            vertices.back().position = p0;
            vertices.back().normal = normal;
            vertices.back().uv = glm::vec2(0.0f, 0.0f);
            vertices.back().tangent = glm::vec4(tangent, 1.0f);

            vertices.push_back({});
            vertices.back().position = p1;
            vertices.back().normal = normal;
            vertices.back().uv = glm::vec2(1.0f, 0.0f);
            vertices.back().tangent = glm::vec4(tangent, 1.0f);

            vertices.push_back({});
            vertices.back().position = p2;
            vertices.back().normal = normal;
            vertices.back().uv = glm::vec2(1.0f, 1.0f);
            vertices.back().tangent = glm::vec4(tangent, 1.0f);

            vertices.push_back({});
            vertices.back().position = p3;
            vertices.back().normal = normal;
            vertices.back().uv = glm::vec2(0.0f, 1.0f);
            vertices.back().tangent = glm::vec4(tangent, 1.0f);

            // Two triangles per face
            indices.push_back(baseIdx + 0);
            indices.push_back(baseIdx + 1);
            indices.push_back(baseIdx + 2);

            indices.push_back(baseIdx + 0);
            indices.push_back(baseIdx + 2);
            indices.push_back(baseIdx + 3);
        };

        // Front face (+Z)
        addFace(
            glm::vec3(-halfSize, -halfSize, halfSize),
            glm::vec3(halfSize, -halfSize, halfSize),
            glm::vec3(halfSize, halfSize, halfSize),
            glm::vec3(-halfSize, halfSize, halfSize),
            glm::vec3(0.0f, 0.0f, 1.0f),   // normal
            glm::vec3(1.0f, 0.0f, 0.0f)    // tangent
        );

        // Back face (-Z)
        addFace(
            glm::vec3(halfSize, -halfSize, -halfSize),
            glm::vec3(-halfSize, -halfSize, -halfSize),
            glm::vec3(-halfSize, halfSize, -halfSize),
            glm::vec3(halfSize, halfSize, -halfSize),
            glm::vec3(0.0f, 0.0f, -1.0f),  // normal
            glm::vec3(-1.0f, 0.0f, 0.0f)   // tangent
        );

        // Right face (+X)
        addFace(
            glm::vec3(halfSize, -halfSize, halfSize),
            glm::vec3(halfSize, -halfSize, -halfSize),
            glm::vec3(halfSize, halfSize, -halfSize),
            glm::vec3(halfSize, halfSize, halfSize),
            glm::vec3(1.0f, 0.0f, 0.0f),   // normal
            glm::vec3(0.0f, 0.0f, -1.0f)   // tangent
        );

        // Left face (-X)
        addFace(
            glm::vec3(-halfSize, -halfSize, -halfSize),
            glm::vec3(-halfSize, -halfSize, halfSize),
            glm::vec3(-halfSize, halfSize, halfSize),
            glm::vec3(-halfSize, halfSize, -halfSize),
            glm::vec3(-1.0f, 0.0f, 0.0f),  // normal
            glm::vec3(0.0f, 0.0f, 1.0f)    // tangent
        );

        // Top face (+Y)
        addFace(
            glm::vec3(-halfSize, halfSize, halfSize),
            glm::vec3(halfSize, halfSize, halfSize),
            glm::vec3(halfSize, halfSize, -halfSize),
            glm::vec3(-halfSize, halfSize, -halfSize),
            glm::vec3(0.0f, 1.0f, 0.0f),   // normal
            glm::vec3(1.0f, 0.0f, 0.0f)    // tangent
        );

        // Bottom face (-Y)
        addFace(
            glm::vec3(-halfSize, -halfSize, -halfSize),
            glm::vec3(halfSize, -halfSize, -halfSize),
            glm::vec3(halfSize, -halfSize, halfSize),
            glm::vec3(-halfSize, -halfSize, halfSize),
            glm::vec3(0.0f, -1.0f, 0.0f),  // normal
            glm::vec3(1.0f, 0.0f, 0.0f)    // tangent
        );

        return {vertices, indices};
    }

    /**
     * Create a UV sphere
     *
     * @param radius Sphere radius
     * @param segments Number of horizontal segments (longitude)
     * @param rings Number of vertical rings (latitude)
     */
    static std::pair<std::vector<Vertex>, std::vector<uint32_t>> createSphere(
        float radius = 0.5f,
        uint32_t segments = 32,
        uint32_t rings = 16
    ) {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;

        // Generate vertices
        for (uint32_t ring = 0; ring <= rings; ++ring) {
            float phi = glm::pi<float>() * static_cast<float>(ring) / static_cast<float>(rings);
            float sinPhi = std::sin(phi);
            float cosPhi = std::cos(phi);

            for (uint32_t seg = 0; seg <= segments; ++seg) {
                float theta = 2.0f * glm::pi<float>() * static_cast<float>(seg) / static_cast<float>(segments);
                float sinTheta = std::sin(theta);
                float cosTheta = std::cos(theta);

                Vertex v;
                v.position = glm::vec3(
                    radius * sinPhi * cosTheta,
                    radius * cosPhi,
                    radius * sinPhi * sinTheta
                );
                v.normal = glm::normalize(v.position);
                v.uv = glm::vec2(
                    static_cast<float>(seg) / static_cast<float>(segments),
                    static_cast<float>(ring) / static_cast<float>(rings)
                );

                // Tangent points along longitude
                v.tangent = glm::vec4(glm::normalize(glm::vec3(-sinTheta, 0.0f, cosTheta)), 1.0f);

                vertices.push_back(v);
            }
        }

        // Generate indices
        for (uint32_t ring = 0; ring < rings; ++ring) {
            for (uint32_t seg = 0; seg < segments; ++seg) {
                uint32_t current = ring * (segments + 1) + seg;
                uint32_t next = current + segments + 1;

                indices.push_back(current);
                indices.push_back(next);
                indices.push_back(current + 1);

                indices.push_back(current + 1);
                indices.push_back(next);
                indices.push_back(next + 1);
            }
        }

        return {vertices, indices};
    }

    /**
     * Create a ground plane (XZ plane, Y-up)
     *
     * @param width Width along X axis
     * @param depth Depth along Z axis
     * @param subdivisionsX Number of subdivisions along X
     * @param subdivisionsZ Number of subdivisions along Z
     */
    static std::pair<std::vector<Vertex>, std::vector<uint32_t>> createPlane(
        float width = 10.0f,
        float depth = 10.0f,
        uint32_t subdivisionsX = 10,
        uint32_t subdivisionsZ = 10
    ) {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;

        float halfW = width * 0.5f;
        float halfD = depth * 0.5f;

        // Generate vertices
        for (uint32_t z = 0; z <= subdivisionsZ; ++z) {
            for (uint32_t x = 0; x <= subdivisionsX; ++x) {
                float fx = static_cast<float>(x) / static_cast<float>(subdivisionsX);
                float fz = static_cast<float>(z) / static_cast<float>(subdivisionsZ);

                Vertex v;
                v.position = glm::vec3(
                    -halfW + fx * width,
                    0.0f,
                    -halfD + fz * depth
                );
                v.normal = glm::vec3(0.0f, 1.0f, 0.0f);
                v.uv = glm::vec2(fx, fz);
                v.tangent = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);

                vertices.push_back(v);
            }
        }

        // Generate indices
        for (uint32_t z = 0; z < subdivisionsZ; ++z) {
            for (uint32_t x = 0; x < subdivisionsX; ++x) {
                uint32_t topLeft = z * (subdivisionsX + 1) + x;
                uint32_t topRight = topLeft + 1;
                uint32_t bottomLeft = topLeft + (subdivisionsX + 1);
                uint32_t bottomRight = bottomLeft + 1;

                indices.push_back(topLeft);
                indices.push_back(bottomLeft);
                indices.push_back(topRight);

                indices.push_back(topRight);
                indices.push_back(bottomLeft);
                indices.push_back(bottomRight);
            }
        }

        return {vertices, indices};
    }

    /**
     * Create a cylinder (Y-axis aligned)
     *
     * @param radius Cylinder radius
     * @param height Cylinder height
     * @param segments Number of segments around circumference
     * @param includeCaps Whether to include top/bottom caps
     */
    static std::pair<std::vector<Vertex>, std::vector<uint32_t>> createCylinder(
        float radius = 0.5f,
        float height = 1.0f,
        uint32_t segments = 32,
        bool includeCaps = true
    ) {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;

        float halfHeight = height * 0.5f;

        // Generate side vertices (duplicated for proper normals)
        for (uint32_t i = 0; i <= segments; ++i) {
            float theta = 2.0f * glm::pi<float>() * static_cast<float>(i) / static_cast<float>(segments);
            float cosTheta = std::cos(theta);
            float sinTheta = std::sin(theta);

            glm::vec3 normal(cosTheta, 0.0f, sinTheta);
            glm::vec3 tangent(-sinTheta, 0.0f, cosTheta);

            // Bottom vertex
            Vertex vBottom;
            vBottom.position = glm::vec3(radius * cosTheta, -halfHeight, radius * sinTheta);
            vBottom.normal = normal;
            vBottom.uv = glm::vec2(static_cast<float>(i) / static_cast<float>(segments), 0.0f);
            vBottom.tangent = glm::vec4(tangent, 1.0f);
            vertices.push_back(vBottom);

            // Top vertex
            Vertex vTop;
            vTop.position = glm::vec3(radius * cosTheta, halfHeight, radius * sinTheta);
            vTop.normal = normal;
            vTop.uv = glm::vec2(static_cast<float>(i) / static_cast<float>(segments), 1.0f);
            vTop.tangent = glm::vec4(tangent, 1.0f);
            vertices.push_back(vTop);
        }

        // Generate side indices
        for (uint32_t i = 0; i < segments; ++i) {
            uint32_t i0 = i * 2;
            uint32_t i1 = i0 + 1;
            uint32_t i2 = i0 + 2;
            uint32_t i3 = i0 + 3;

            indices.push_back(i0);
            indices.push_back(i2);
            indices.push_back(i1);

            indices.push_back(i1);
            indices.push_back(i2);
            indices.push_back(i3);
        }

        if (includeCaps) {
            // Top cap
            uint32_t topCenterIdx = static_cast<uint32_t>(vertices.size());
            Vertex topCenter;
            topCenter.position = glm::vec3(0.0f, halfHeight, 0.0f);
            topCenter.normal = glm::vec3(0.0f, 1.0f, 0.0f);
            topCenter.uv = glm::vec2(0.5f, 0.5f);
            topCenter.tangent = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
            vertices.push_back(topCenter);

            for (uint32_t i = 0; i < segments; ++i) {
                float theta = 2.0f * glm::pi<float>() * static_cast<float>(i) / static_cast<float>(segments);
                float cosTheta = std::cos(theta);
                float sinTheta = std::sin(theta);

                Vertex v;
                v.position = glm::vec3(radius * cosTheta, halfHeight, radius * sinTheta);
                v.normal = glm::vec3(0.0f, 1.0f, 0.0f);
                v.uv = glm::vec2(0.5f + 0.5f * cosTheta, 0.5f + 0.5f * sinTheta);
                v.tangent = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
                vertices.push_back(v);
            }

            for (uint32_t i = 0; i < segments; ++i) {
                uint32_t next = (i + 1) % segments;
                indices.push_back(topCenterIdx);
                indices.push_back(topCenterIdx + 1 + i);
                indices.push_back(topCenterIdx + 1 + next);
            }

            // Bottom cap
            uint32_t bottomCenterIdx = static_cast<uint32_t>(vertices.size());
            Vertex bottomCenter;
            bottomCenter.position = glm::vec3(0.0f, -halfHeight, 0.0f);
            bottomCenter.normal = glm::vec3(0.0f, -1.0f, 0.0f);
            bottomCenter.uv = glm::vec2(0.5f, 0.5f);
            bottomCenter.tangent = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
            vertices.push_back(bottomCenter);

            for (uint32_t i = 0; i < segments; ++i) {
                float theta = 2.0f * glm::pi<float>() * static_cast<float>(i) / static_cast<float>(segments);
                float cosTheta = std::cos(theta);
                float sinTheta = std::sin(theta);

                Vertex v;
                v.position = glm::vec3(radius * cosTheta, -halfHeight, radius * sinTheta);
                v.normal = glm::vec3(0.0f, -1.0f, 0.0f);
                v.uv = glm::vec2(0.5f + 0.5f * cosTheta, 0.5f + 0.5f * sinTheta);
                v.tangent = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
                vertices.push_back(v);
            }

            for (uint32_t i = 0; i < segments; ++i) {
                uint32_t next = (i + 1) % segments;
                indices.push_back(bottomCenterIdx);
                indices.push_back(bottomCenterIdx + 1 + next);
                indices.push_back(bottomCenterIdx + 1 + i);
            }
        }

        return {vertices, indices};
    }

    /**
     * Create a capsule (cylinder with hemispherical caps)
     *
     * @param radius Capsule radius
     * @param height Cylinder height (not including caps)
     * @param segments Number of segments around circumference
     * @param rings Number of rings per hemisphere
     */
    static std::pair<std::vector<Vertex>, std::vector<uint32_t>> createCapsule(
        float radius = 0.5f,
        float height = 1.0f,
        uint32_t segments = 32,
        uint32_t rings = 8
    ) {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;

        float halfHeight = height * 0.5f;

        // Top hemisphere
        for (uint32_t ring = 0; ring <= rings; ++ring) {
            float phi = (glm::pi<float>() * 0.5f) * static_cast<float>(ring) / static_cast<float>(rings);
            float sinPhi = std::sin(phi);
            float cosPhi = std::cos(phi);

            for (uint32_t seg = 0; seg <= segments; ++seg) {
                float theta = 2.0f * glm::pi<float>() * static_cast<float>(seg) / static_cast<float>(segments);
                float sinTheta = std::sin(theta);
                float cosTheta = std::cos(theta);

                Vertex v;
                v.position = glm::vec3(
                    radius * sinPhi * cosTheta,
                    halfHeight + radius * cosPhi,
                    radius * sinPhi * sinTheta
                );
                v.normal = glm::normalize(v.position - glm::vec3(0.0f, halfHeight, 0.0f));
                v.uv = glm::vec2(
                    static_cast<float>(seg) / static_cast<float>(segments),
                    0.5f - (0.5f * static_cast<float>(ring) / static_cast<float>(rings))
                );
                v.tangent = glm::vec4(glm::normalize(glm::vec3(-sinTheta, 0.0f, cosTheta)), 1.0f);

                vertices.push_back(v);
            }
        }

        uint32_t topHemisphereVertCount = (rings + 1) * (segments + 1);

        // Bottom hemisphere
        for (uint32_t ring = 0; ring <= rings; ++ring) {
            float phi = (glm::pi<float>() * 0.5f) + (glm::pi<float>() * 0.5f) * static_cast<float>(ring) / static_cast<float>(rings);
            float sinPhi = std::sin(phi);
            float cosPhi = std::cos(phi);

            for (uint32_t seg = 0; seg <= segments; ++seg) {
                float theta = 2.0f * glm::pi<float>() * static_cast<float>(seg) / static_cast<float>(segments);
                float sinTheta = std::sin(theta);
                float cosTheta = std::cos(theta);

                Vertex v;
                v.position = glm::vec3(
                    radius * sinPhi * cosTheta,
                    -halfHeight + radius * cosPhi,
                    radius * sinPhi * sinTheta
                );
                v.normal = glm::normalize(v.position - glm::vec3(0.0f, -halfHeight, 0.0f));
                v.uv = glm::vec2(
                    static_cast<float>(seg) / static_cast<float>(segments),
                    0.5f + (0.5f * static_cast<float>(ring) / static_cast<float>(rings))
                );
                v.tangent = glm::vec4(glm::normalize(glm::vec3(-sinTheta, 0.0f, cosTheta)), 1.0f);

                vertices.push_back(v);
            }
        }

        // Generate indices for both hemispheres
        for (uint32_t hemi = 0; hemi < 2; ++hemi) {
            uint32_t baseIdx = hemi * topHemisphereVertCount;
            for (uint32_t ring = 0; ring < rings; ++ring) {
                for (uint32_t seg = 0; seg < segments; ++seg) {
                    uint32_t current = baseIdx + ring * (segments + 1) + seg;
                    uint32_t next = current + segments + 1;

                    indices.push_back(current);
                    indices.push_back(next);
                    indices.push_back(current + 1);

                    indices.push_back(current + 1);
                    indices.push_back(next);
                    indices.push_back(next + 1);
                }
            }
        }

        return {vertices, indices};
    }

    /**
     * Create a cone (Y-axis aligned, tip at +Y)
     *
     * @param radius Base radius
     * @param height Cone height
     * @param segments Number of segments around base
     * @param includeCap Whether to include bottom cap
     */
    static std::pair<std::vector<Vertex>, std::vector<uint32_t>> createCone(
        float radius = 0.5f,
        float height = 1.0f,
        uint32_t segments = 32,
        bool includeCap = true
    ) {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;

        float halfHeight = height * 0.5f;

        // Tip vertex
        Vertex tip;
        tip.position = glm::vec3(0.0f, halfHeight, 0.0f);

        // Generate side vertices
        for (uint32_t i = 0; i <= segments; ++i) {
            float theta = 2.0f * glm::pi<float>() * static_cast<float>(i) / static_cast<float>(segments);
            float cosTheta = std::cos(theta);
            float sinTheta = std::sin(theta);

            // Calculate normal (slanted surface)
            glm::vec3 basePos(radius * cosTheta, -halfHeight, radius * sinTheta);
            glm::vec3 toTip = glm::normalize(tip.position - basePos);
            glm::vec3 radial = glm::normalize(glm::vec3(cosTheta, 0.0f, sinTheta));
            glm::vec3 tangent(-sinTheta, 0.0f, cosTheta);
            glm::vec3 normal = glm::normalize(glm::cross(tangent, toTip));

            // Tip vertex for this segment
            Vertex vTip = tip;
            vTip.normal = normal;
            vTip.uv = glm::vec2(static_cast<float>(i) / static_cast<float>(segments), 1.0f);
            vTip.tangent = glm::vec4(tangent, 1.0f);
            vertices.push_back(vTip);

            // Base vertex
            Vertex vBase;
            vBase.position = basePos;
            vBase.normal = normal;
            vBase.uv = glm::vec2(static_cast<float>(i) / static_cast<float>(segments), 0.0f);
            vBase.tangent = glm::vec4(tangent, 1.0f);
            vertices.push_back(vBase);
        }

        // Generate side indices
        for (uint32_t i = 0; i < segments; ++i) {
            uint32_t i0 = i * 2;
            uint32_t i1 = i0 + 1;
            uint32_t i2 = i0 + 2;

            indices.push_back(i0);
            indices.push_back(i1);
            indices.push_back(i2);
        }

        if (includeCap) {
            // Bottom cap
            uint32_t capCenterIdx = static_cast<uint32_t>(vertices.size());
            Vertex capCenter;
            capCenter.position = glm::vec3(0.0f, -halfHeight, 0.0f);
            capCenter.normal = glm::vec3(0.0f, -1.0f, 0.0f);
            capCenter.uv = glm::vec2(0.5f, 0.5f);
            capCenter.tangent = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
            vertices.push_back(capCenter);

            for (uint32_t i = 0; i < segments; ++i) {
                float theta = 2.0f * glm::pi<float>() * static_cast<float>(i) / static_cast<float>(segments);
                float cosTheta = std::cos(theta);
                float sinTheta = std::sin(theta);

                Vertex v;
                v.position = glm::vec3(radius * cosTheta, -halfHeight, radius * sinTheta);
                v.normal = glm::vec3(0.0f, -1.0f, 0.0f);
                v.uv = glm::vec2(0.5f + 0.5f * cosTheta, 0.5f + 0.5f * sinTheta);
                v.tangent = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
                vertices.push_back(v);
            }

            for (uint32_t i = 0; i < segments; ++i) {
                uint32_t next = (i + 1) % segments;
                indices.push_back(capCenterIdx);
                indices.push_back(capCenterIdx + 1 + next);
                indices.push_back(capCenterIdx + 1 + i);
            }
        }

        return {vertices, indices};
    }
};

} // namespace hvk

#endif // HVK_PRIMITIVES_H
