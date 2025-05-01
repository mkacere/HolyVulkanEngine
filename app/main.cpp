// main.cpp

#include <iostream>
#include <vector>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "hvk_window.h"
#include "hvk_device.h"
#include "hvk_swap_chain.h"
#include "hvk_buffer.h"
#include "hvk_descriptors.h"

struct Vertex {
    float position[2];
    float color[3];
};

int main() {
    // 1) Create a window
    hvk::HvkWindow window{ 600, 600, "My HVK Window" };

    // 2) Create Vulkan device (instance, physical+logical device, queues, command pool)
    hvk::HvkDevice device{ window };

    // 3) Create swap‐chain (images, views, MSAA/color/depth, render-pass, framebuffers, sync)
    VkExtent2D extent{ 600, 600 };
    auto swapChain = std::make_shared<hvk::HvkSwapChain>(device, extent);

    // 4) Upload a simple triangle into a host-visible vertex buffer
    std::vector<Vertex> vertices = {
        {{ 0.0f, -0.5f }, {1.0f, 0.0f, 0.0f}},
        {{ 0.5f,  0.5f }, {0.0f, 1.0f, 0.0f}},
        {{-0.5f,  0.5f }, {0.0f, 0.0f, 1.0f}},
    };
    hvk::HvkBuffer vertexBuffer{
        device,
        sizeof(Vertex),
        static_cast<uint32_t>(vertices.size()),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    };
    vertexBuffer.map();
    vertexBuffer.writeToBuffer(vertices.data());
    vertexBuffer.flush();
    vertexBuffer.unmap();

    // 5) Build a descriptor‐set layout + pool + allocate & write one set
    auto descriptorSetLayout = hvk::HvkDescriptorSetLayout::Builder{ device }
        .addBinding(
            /*binding=*/0,
            /*type=*/VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            /*stageFlags=*/VK_SHADER_STAGE_VERTEX_BIT
        )
        .build();

    auto descriptorPool = hvk::HvkDescriptorPool::Builder{ device }
        .addPoolSize(
            /*type=*/VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            /*count=*/1
        )
        .setMaxSets(1)
        .build();

    VkDescriptorSet descriptorSet;
    VkDescriptorBufferInfo bufferInfo = vertexBuffer.descriptorInfo();

    hvk::HvkDescriptorWriter writer{ *descriptorSetLayout, *descriptorPool };
    writer
        .writeBuffer(0, &bufferInfo)
        .build(descriptorSet);

    // 6) Main loop (no pipeline yet)
    while (!glfwWindowShouldClose(window.getGLFWwindow())) {
        glfwPollEvents();
        // Here you would acquire, record, submit & present
    }

    vkDeviceWaitIdle(device.device());
    //std::cout << "Shutdown cleanly!\n";
    return 0;
}
