/**
 * @file hvk_raii.hpp
 * @brief CRTP-based RAII wrapper for Vulkan resources
 * @author Holy Vulkan Engine
 * @date 2025
 *
 * Provides a base template for consistent RAII semantics across all Vulkan resources.
 * Uses the Curiously Recurring Template Pattern (CRTP) for zero-overhead abstraction.
 */

#ifndef HVK_RAII_HPP
#define HVK_RAII_HPP

#include <utility> // for std::move

namespace hvk {

/**
 * @brief CRTP base class providing RAII semantics for Vulkan resources
 *
 * This template eliminates boilerplate for move constructors, move assignment,
 * and destructors across all Vulkan resource wrapper classes.
 *
 * @tparam Derived The derived class type (CRTP pattern)
 *
 * Requirements for Derived:
 * - Must implement: void destroy()
 * - Must implement: void move_from(Derived&& other) noexcept
 * - Must be friends with VulkanResource<Derived>
 *
 * Example usage:
 * @code
 * class GpuBuffer : public VulkanResource<GpuBuffer> {
 *     friend class VulkanResource<GpuBuffer>;
 * public:
 *     GpuBuffer() = default;
 *     // ... rest of interface ...
 * private:
 *     void destroy();
 *     void move_from(GpuBuffer&& o) noexcept;
 * };
 * @endcode
 */
template<typename Derived>
class VulkanResource {
public:
    /**
     * @brief Destructor - calls derived class destroy()
     *
     * Automatically invokes the derived class's destroy() method,
     * ensuring proper cleanup of Vulkan resources.
     */
    ~VulkanResource() {
        static_cast<Derived*>(this)->destroy();
    }

    // Non-copyable (Vulkan handles cannot be copied)
    VulkanResource(const VulkanResource&) = delete;
    VulkanResource& operator=(const VulkanResource&) = delete;

    // Movable (derived classes must implement move operations explicitly)
    VulkanResource(VulkanResource&&) noexcept = default;
    VulkanResource& operator=(VulkanResource&&) noexcept = default;

protected:
    VulkanResource() = default;
};

} // namespace hvk

#endif // HVK_RAII_HPP
