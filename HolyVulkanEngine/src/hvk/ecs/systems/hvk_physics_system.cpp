#include <hvk/ecs/systems/hvk_physics_system.hpp>
#include <hvk/ecs/systems/hvk_debug_line_render_system.hpp>
#include <hvk/ecs/hvk_scene.hpp>
#include <hvk/ecs/hvk_components.hpp>

#include <cstdarg>
#include <thread>

// Jolt Physics includes
#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>

#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

// Jolt uses Trace for errors and asserts for fatal errors
#ifdef JPH_ENABLE_ASSERTS
    #undef JPH_ENABLE_ASSERTS
#endif

namespace hvk {

// Jolt callback for traces (errors)
static void TraceImpl(const char* inFMT, ...) {
    va_list list;
    va_start(list, inFMT);
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), inFMT, list);
    va_end(list);
    std::cout << "[Jolt] " << buffer << std::endl;
}

#ifdef JPH_ENABLE_ASSERTS
// Jolt callback for asserts (fatal errors)
static bool AssertFailedImpl(const char* inExpression, const char* inMessage, const char* inFile, uint32_t inLine) {
    std::cout << "[Jolt Assert] " << inFile << ":" << inLine << ": " << inExpression << " - " << (inMessage ? inMessage : "") << std::endl;
    return true; // Trigger breakpoint
}
#endif

// Collision layers
namespace Layers {
    static constexpr uint8_t NON_MOVING = 0;
    static constexpr uint8_t MOVING = 1;
    static constexpr uint8_t NUM_LAYERS = 2;
}

// Layer pair filter
class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter {
public:
    virtual bool ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const override {
        switch (inObject1) {
            case Layers::NON_MOVING:
                return inObject2 == Layers::MOVING; // Non-moving only collides with moving
            case Layers::MOVING:
                return true; // Moving collides with everything
            default:
                return false;
        }
    }
};

// Broad phase layer interface
class BroadPhaseLayerInterfaceImpl : public JPH::BroadPhaseLayerInterface {
public:
    BroadPhaseLayerInterfaceImpl() {
        // Create mapping table from object layer to broad phase layer
        mObjectToBroadPhase[Layers::NON_MOVING] = JPH::BroadPhaseLayer(BroadPhaseLayers::NON_MOVING);
        mObjectToBroadPhase[Layers::MOVING] = JPH::BroadPhaseLayer(BroadPhaseLayers::MOVING);
    }

    virtual uint32_t GetNumBroadPhaseLayers() const override {
        return BroadPhaseLayers::NUM_LAYERS;
    }

    virtual JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override {
        return mObjectToBroadPhase[inLayer];
    }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    virtual const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override {
        switch ((JPH::BroadPhaseLayer::Type)inLayer) {
            case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::NON_MOVING: return "NON_MOVING";
            case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::MOVING: return "MOVING";
            default: return "INVALID";
        }
    }
#endif

private:
    enum BroadPhaseLayers {
        NON_MOVING = 0,
        MOVING = 1,
        NUM_LAYERS = 2
    };

    JPH::BroadPhaseLayer mObjectToBroadPhase[Layers::NUM_LAYERS];
};

// Broad phase layer pair filter
class ObjectVsBroadPhaseLayerFilterImpl : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
    virtual bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override {
        switch (inLayer1) {
            case Layers::NON_MOVING:
                return inLayer2 == JPH::BroadPhaseLayer(1); // MOVING
            case Layers::MOVING:
                return true;
            default:
                return false;
        }
    }
};

// =============================================================================
// PhysicsSystem Implementation
// =============================================================================

PhysicsSystem::PhysicsSystem() = default;

PhysicsSystem::~PhysicsSystem() {
    cleanup();
}

void PhysicsSystem::init(Scene& scene) {
    // Register Jolt allocation hooks
    JPH::RegisterDefaultAllocator();

    // Install callbacks
    JPH::Trace = TraceImpl;
#ifdef JPH_ENABLE_ASSERTS
    JPH::AssertFailed = AssertFailedImpl;
#endif

    // Register all Jolt types
    JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();

    // Create temp allocator (10 MB)
    tempAllocator_ = std::make_unique<JPH::TempAllocatorImpl>(10 * 1024 * 1024);

    // Create job system (use all available hardware threads)
    const uint32_t maxJobs = 1024;
    const uint32_t maxBarriers = 8;
    const int numThreads = std::thread::hardware_concurrency() - 1; // Leave one for main thread
    jobSystem_ = std::make_unique<JPH::JobSystemThreadPool>(maxJobs, maxBarriers, numThreads);

    // Create layer filter interfaces (must persist for lifetime of physics system)
    broadPhaseLayerInterface_ = std::make_unique<BroadPhaseLayerInterfaceImpl>();
    objectVsBroadPhaseLayerFilter_ = std::make_unique<ObjectVsBroadPhaseLayerFilterImpl>();
    objectLayerPairFilter_ = std::make_unique<ObjectLayerPairFilterImpl>();

    // Create physics system
    const uint32_t maxBodies = 10240;
    const uint32_t numBodyMutexes = 0; // Auto-detect
    const uint32_t maxBodyPairs = 65536;
    const uint32_t maxContactConstraints = 10240;

    physicsSystem_ = std::make_unique<JPH::PhysicsSystem>();
    physicsSystem_->Init(
        maxBodies,
        numBodyMutexes,
        maxBodyPairs,
        maxContactConstraints,
        *broadPhaseLayerInterface_,
        *objectVsBroadPhaseLayerFilter_,
        *objectLayerPairFilter_
    );

    // Set gravity
    physicsSystem_->SetGravity(JPH::Vec3(0.0f, -9.81f, 0.0f));
}

void PhysicsSystem::update(Scene& scene, float deltaTime) {
    entt::registry& registry = scene.registry();
    JPH::BodyInterface& bodyInterface = physicsSystem_->GetBodyInterface();

    // 1. Create Jolt bodies for new entities with RigidBody + Collider + Transform
    auto newBodiesView = registry.view<RigidBodyComponent, ColliderComponent, TransformComponent>();
    for (auto entity : newBodiesView) {
        auto& rb = newBodiesView.get<RigidBodyComponent>(entity);

        // Skip if already created
        if (rb.joltBodyID != 0xFFFFFFFF) {
            continue;
        }

        createBody(registry, entity);
    }

    // 2. Update kinematic bodies from ECS transforms
    auto kinematicView = registry.view<RigidBodyComponent, TransformComponent>();
    for (auto entity : kinematicView) {
        auto& rb = kinematicView.get<RigidBodyComponent>(entity);
        if (rb.type == RigidBodyComponent::Type::Kinematic && rb.joltBodyID != 0xFFFFFFFF) {
            updateKinematicBody(registry, entity);
        }
    }

    // 3. Step physics simulation
    const int collisionSteps = 1;
    physicsSystem_->Update(deltaTime, collisionSteps, tempAllocator_.get(), jobSystem_.get());

    // 4. Sync dynamic bodies back to ECS
    auto dynamicView = registry.view<RigidBodyComponent, TransformComponent>();
    for (auto entity : dynamicView) {
        auto& rb = dynamicView.get<RigidBodyComponent>(entity);
        if (rb.type == RigidBodyComponent::Type::Dynamic && rb.joltBodyID != 0xFFFFFFFF) {
            syncBodyToECS(registry, entity);
        }
    }

    // 5. Update velocity components if present
    auto velocityView = registry.view<RigidBodyComponent, VelocityComponent>();
    for (auto entity : velocityView) {
        auto& rb = velocityView.get<RigidBodyComponent>(entity);
        auto& vel = velocityView.get<VelocityComponent>(entity);

        if (rb.joltBodyID == 0xFFFFFFFF) {
            continue;
        }

        JPH::BodyID bodyID(rb.joltBodyID);
        JPH::Vec3 linearVel = bodyInterface.GetLinearVelocity(bodyID);
        JPH::Vec3 angularVel = bodyInterface.GetAngularVelocity(bodyID);

        vel.linear = glm::vec3(linearVel.GetX(), linearVel.GetY(), linearVel.GetZ());
        vel.angular = glm::vec3(angularVel.GetX(), angularVel.GetY(), angularVel.GetZ());
    }

    // 6. Debug visualization (after physics step to prevent flickering)
    if (debugDrawEnabled_ && debugLines_) {
        debugLines_->clearLines();  // Clear old lines before adding new ones
        drawDebugVisualization(registry);
    }
}


void PhysicsSystem::cleanup() {
    if (physicsSystem_) {
        // Remove all bodies
        JPH::BodyInterface& bodyInterface = physicsSystem_->GetBodyInterface();
        JPH::BodyIDVector bodyIDs;
        physicsSystem_->GetBodies(bodyIDs);

        for (JPH::BodyID bodyID : bodyIDs) {
            bodyInterface.RemoveBody(bodyID);
            bodyInterface.DestroyBody(bodyID);
        }
    }

    // Cleanup in reverse order of creation
    physicsSystem_.reset();
    jobSystem_.reset();
    tempAllocator_.reset();

    // Unregister types
    if (JPH::Factory::sInstance) {
        delete JPH::Factory::sInstance;
        JPH::Factory::sInstance = nullptr;
    }
}

void PhysicsSystem::createBody(entt::registry& registry, entt::entity entity) {
    auto& rb = registry.get<RigidBodyComponent>(entity);
    auto& collider = registry.get<ColliderComponent>(entity);
    auto& transform = registry.get<TransformComponent>(entity);

    // Create shape
    JPH::ShapeRefC shape;
    switch (collider.shape) {
        case ColliderComponent::Shape::Box:
            shape = new JPH::BoxShape(JPH::Vec3(collider.size.x, collider.size.y, collider.size.z));
            break;
        case ColliderComponent::Shape::Sphere:
            shape = new JPH::SphereShape(collider.size.x);
            break;
        case ColliderComponent::Shape::Capsule:
            shape = new JPH::CapsuleShape(collider.size.y * 0.5f, collider.size.x);
            break;
        case ColliderComponent::Shape::Cylinder:
            shape = new JPH::CylinderShape(collider.size.y * 0.5f, collider.size.x);
            break;
        default:
            shape = new JPH::BoxShape(JPH::Vec3(1.0f, 1.0f, 1.0f));
            break;
    }

    // Convert ECS transform to Jolt
    JPH::Vec3 position(transform.position.x, transform.position.y, transform.position.z);
    JPH::Quat rotation(transform.rotation.x, transform.rotation.y, transform.rotation.z, transform.rotation.w);

    // Determine motion type
    JPH::EMotionType motionType;
    JPH::ObjectLayer objectLayer;
    switch (rb.type) {
        case RigidBodyComponent::Type::Static:
            motionType = JPH::EMotionType::Static;
            objectLayer = Layers::NON_MOVING;
            break;
        case RigidBodyComponent::Type::Dynamic:
            motionType = JPH::EMotionType::Dynamic;
            objectLayer = Layers::MOVING;
            break;
        case RigidBodyComponent::Type::Kinematic:
            motionType = JPH::EMotionType::Kinematic;
            objectLayer = Layers::MOVING;
            break;
        default:
            motionType = JPH::EMotionType::Dynamic;
            objectLayer = Layers::MOVING;
            break;
    }

    // Create body settings
    JPH::BodyCreationSettings bodySettings(
        shape,
        position,
        rotation,
        motionType,
        objectLayer
    );

    // Apply physics properties
    bodySettings.mFriction = rb.friction;
    bodySettings.mRestitution = rb.restitution;
    bodySettings.mLinearDamping = rb.linearDamping;
    bodySettings.mAngularDamping = rb.angularDamping;
    bodySettings.mGravityFactor = rb.useGravity ? 1.0f : 0.0f;
    bodySettings.mIsSensor = rb.isSensor;

    if (rb.type == RigidBodyComponent::Type::Dynamic) {
        bodySettings.mMassPropertiesOverride.mMass = rb.mass;
    }

    // Create body
    JPH::BodyInterface& bodyInterface = physicsSystem_->GetBodyInterface();
    JPH::Body* body = bodyInterface.CreateBody(bodySettings);

    if (!body) {
        std::cerr << "[PhysicsSystem] Failed to create body for entity " << static_cast<uint32_t>(entity) << std::endl;
        return;
    }

    JPH::BodyID bodyID = body->GetID();
    rb.joltBodyID = bodyID.GetIndexAndSequenceNumber();

    // Add to physics system
    bodyInterface.AddBody(bodyID, JPH::EActivation::Activate);
}

void PhysicsSystem::updateKinematicBody(entt::registry& registry, entt::entity entity) {
    auto& rb = registry.get<RigidBodyComponent>(entity);
    auto& transform = registry.get<TransformComponent>(entity);

    JPH::BodyID bodyID(rb.joltBodyID);
    JPH::BodyInterface& bodyInterface = physicsSystem_->GetBodyInterface();

    JPH::Vec3 position(transform.position.x, transform.position.y, transform.position.z);
    JPH::Quat rotation(transform.rotation.x, transform.rotation.y, transform.rotation.z, transform.rotation.w);

    bodyInterface.SetPositionAndRotation(bodyID, position, rotation, JPH::EActivation::Activate);
}

void PhysicsSystem::syncBodyToECS(entt::registry& registry, entt::entity entity) {
    auto& rb = registry.get<RigidBodyComponent>(entity);
    auto& transform = registry.get<TransformComponent>(entity);

    JPH::BodyID bodyID(rb.joltBodyID);
    JPH::BodyInterface& bodyInterface = physicsSystem_->GetBodyInterface();

    JPH::Vec3 position = bodyInterface.GetPosition(bodyID);
    JPH::Quat rotation = bodyInterface.GetRotation(bodyID);

    transform.position = glm::vec3(position.GetX(), position.GetY(), position.GetZ());
    transform.rotation = glm::quat(rotation.GetW(), rotation.GetX(), rotation.GetY(), rotation.GetZ());
}

void PhysicsSystem::drawDebugVisualization(entt::registry& registry) {
    auto view = registry.view<RigidBodyComponent, ColliderComponent, TransformComponent>();

    for (auto entity : view) {
        auto& rb = view.get<RigidBodyComponent>(entity);
        auto& collider = view.get<ColliderComponent>(entity);
        auto& transform = view.get<TransformComponent>(entity);

        // Color based on type
        glm::vec3 color;
        switch (rb.type) {
            case RigidBodyComponent::Type::Static:
                color = glm::vec3(0.0f, 1.0f, 0.0f); // Green
                break;
            case RigidBodyComponent::Type::Dynamic:
                color = glm::vec3(1.0f, 1.0f, 0.0f); // Yellow
                break;
            case RigidBodyComponent::Type::Kinematic:
                color = glm::vec3(0.0f, 1.0f, 1.0f); // Cyan
                break;
        }

        // Draw shape based on collider type
        glm::vec3 pos = transform.position + collider.center;

        switch (collider.shape) {
            case ColliderComponent::Shape::Box: {
                // Draw wireframe box
                glm::vec3 halfExtents = collider.size;

                // 8 corners of the box
                glm::vec3 corners[8] = {
                    pos + glm::vec3(-halfExtents.x, -halfExtents.y, -halfExtents.z),
                    pos + glm::vec3( halfExtents.x, -halfExtents.y, -halfExtents.z),
                    pos + glm::vec3( halfExtents.x,  halfExtents.y, -halfExtents.z),
                    pos + glm::vec3(-halfExtents.x,  halfExtents.y, -halfExtents.z),
                    pos + glm::vec3(-halfExtents.x, -halfExtents.y,  halfExtents.z),
                    pos + glm::vec3( halfExtents.x, -halfExtents.y,  halfExtents.z),
                    pos + glm::vec3( halfExtents.x,  halfExtents.y,  halfExtents.z),
                    pos + glm::vec3(-halfExtents.x,  halfExtents.y,  halfExtents.z)
                };

                // 12 edges
                debugLines_->addLine(corners[0], corners[1], color);
                debugLines_->addLine(corners[1], corners[2], color);
                debugLines_->addLine(corners[2], corners[3], color);
                debugLines_->addLine(corners[3], corners[0], color);

                debugLines_->addLine(corners[4], corners[5], color);
                debugLines_->addLine(corners[5], corners[6], color);
                debugLines_->addLine(corners[6], corners[7], color);
                debugLines_->addLine(corners[7], corners[4], color);

                debugLines_->addLine(corners[0], corners[4], color);
                debugLines_->addLine(corners[1], corners[5], color);
                debugLines_->addLine(corners[2], corners[6], color);
                debugLines_->addLine(corners[3], corners[7], color);
                break;
            }

            case ColliderComponent::Shape::Sphere: {
                // Draw 3 circles for sphere
                float radius = collider.size.x;
                const int segments = 16;

                // XY circle
                for (int i = 0; i < segments; ++i) {
                    float angle1 = (float)i / segments * 2.0f * glm::pi<float>();
                    float angle2 = (float)(i + 1) / segments * 2.0f * glm::pi<float>();
                    glm::vec3 p1 = pos + glm::vec3(radius * cos(angle1), radius * sin(angle1), 0.0f);
                    glm::vec3 p2 = pos + glm::vec3(radius * cos(angle2), radius * sin(angle2), 0.0f);
                    debugLines_->addLine(p1, p2, color);
                }

                // XZ circle
                for (int i = 0; i < segments; ++i) {
                    float angle1 = (float)i / segments * 2.0f * glm::pi<float>();
                    float angle2 = (float)(i + 1) / segments * 2.0f * glm::pi<float>();
                    glm::vec3 p1 = pos + glm::vec3(radius * cos(angle1), 0.0f, radius * sin(angle1));
                    glm::vec3 p2 = pos + glm::vec3(radius * cos(angle2), 0.0f, radius * sin(angle2));
                    debugLines_->addLine(p1, p2, color);
                }

                // YZ circle
                for (int i = 0; i < segments; ++i) {
                    float angle1 = (float)i / segments * 2.0f * glm::pi<float>();
                    float angle2 = (float)(i + 1) / segments * 2.0f * glm::pi<float>();
                    glm::vec3 p1 = pos + glm::vec3(0.0f, radius * cos(angle1), radius * sin(angle1));
                    glm::vec3 p2 = pos + glm::vec3(0.0f, radius * cos(angle2), radius * sin(angle2));
                    debugLines_->addLine(p1, p2, color);
                }
                break;
            }

            case ColliderComponent::Shape::Capsule:
            case ColliderComponent::Shape::Cylinder: {
                // Draw simplified cylinder/capsule (vertical orientation)
                float radius = collider.size.x;
                float halfHeight = collider.size.y * 0.5f;
                const int segments = 16;

                // Top circle
                for (int i = 0; i < segments; ++i) {
                    float angle1 = (float)i / segments * 2.0f * glm::pi<float>();
                    float angle2 = (float)(i + 1) / segments * 2.0f * glm::pi<float>();
                    glm::vec3 p1 = pos + glm::vec3(radius * cos(angle1), halfHeight, radius * sin(angle1));
                    glm::vec3 p2 = pos + glm::vec3(radius * cos(angle2), halfHeight, radius * sin(angle2));
                    debugLines_->addLine(p1, p2, color);
                }

                // Bottom circle
                for (int i = 0; i < segments; ++i) {
                    float angle1 = (float)i / segments * 2.0f * glm::pi<float>();
                    float angle2 = (float)(i + 1) / segments * 2.0f * glm::pi<float>();
                    glm::vec3 p1 = pos + glm::vec3(radius * cos(angle1), -halfHeight, radius * sin(angle1));
                    glm::vec3 p2 = pos + glm::vec3(radius * cos(angle2), -halfHeight, radius * sin(angle2));
                    debugLines_->addLine(p1, p2, color);
                }

                // 4 vertical lines
                debugLines_->addLine(pos + glm::vec3( radius,  halfHeight, 0.0f), pos + glm::vec3( radius, -halfHeight, 0.0f), color);
                debugLines_->addLine(pos + glm::vec3(-radius,  halfHeight, 0.0f), pos + glm::vec3(-radius, -halfHeight, 0.0f), color);
                debugLines_->addLine(pos + glm::vec3(0.0f,  halfHeight,  radius), pos + glm::vec3(0.0f, -halfHeight,  radius), color);
                debugLines_->addLine(pos + glm::vec3(0.0f,  halfHeight, -radius), pos + glm::vec3(0.0f, -halfHeight, -radius), color);
                break;
            }
        }
    }
}

} // namespace hvk
