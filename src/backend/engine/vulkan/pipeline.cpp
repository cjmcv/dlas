#include "pipeline.hpp"

namespace ecas {
namespace vulkan {

namespace {

struct SpecConstantData {
    // All packed specialization data
    std::vector<uint8_t> data;
    // Entry describing each specialization constant
    std::vector<VkSpecializationMapEntry> entries;
};

// Packs |spec_constants| into a byte buffer so that they can used for Vulkan
// API calls.
SpecConstantData PackSpecConstantData(
    std::vector<Pipeline::SpecConstant> &spec_constants) {
    size_t total_size = 0;
    for (const auto &spec_const : spec_constants) {
        total_size += spec_const.size();
    }

    std::vector<uint8_t> data(total_size);
    std::vector<VkSpecializationMapEntry> entries;
    entries.reserve(spec_constants.size());

    uint32_t index = 0;  // Next available byte's index in the buffer
    for (const auto &spec_const : spec_constants) {
        uint8_t *ptr = data.data() + index;
        switch (spec_const.type) {
        case Pipeline::SpecConstant::Type::s32: {
            *reinterpret_cast<int32_t *>(ptr) = spec_const.value.s32;
        } break;
        case Pipeline::SpecConstant::Type::u32: {
            *reinterpret_cast<uint32_t *>(ptr) = spec_const.value.u32;
        } break;
        case Pipeline::SpecConstant::Type::f32: {
            *reinterpret_cast<float *>(ptr) = spec_const.value.f32;
        } break;
        }
        entries.emplace_back();
        entries.back().constantID = spec_const.id;
        entries.back().offset = index;
        entries.back().size = spec_const.size();

        index += spec_const.size();
    }

    return SpecConstantData{std::move(data), std::move(entries)};
}

}  // namespace

size_t Pipeline::SpecConstant::size() const {
    switch (type) {
        case Type::s32:
            return sizeof(int32_t);
        case Type::u32:
            return sizeof(uint32_t);
        case Type::f32:
            return sizeof(float);
    }
}

Pipeline *Pipeline::Create(VkDevice device, const ShaderModule &shader_module, 
                           const char *entry_point, 
                           std::vector<Pipeline::SpecConstant> &spec_constants) {
  
    // Pack the specialization constant into an byte buffer
    SpecConstantData spec_constant_data = PackSpecConstantData(spec_constants);
    VkSpecializationInfo spec_constant_info = {};

    VkPipelineShaderStageCreateInfo shader_stage_create_info = {};
    shader_stage_create_info.sType =
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stage_create_info.pNext = nullptr;
    shader_stage_create_info.flags = 0;
    shader_stage_create_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    shader_stage_create_info.module = shader_module.shader_module();
    shader_stage_create_info.pName = entry_point;

    // Update specialization information
    if (!spec_constants.empty()) {
        spec_constant_info.mapEntryCount = spec_constant_data.entries.size();
        spec_constant_info.pMapEntries = spec_constant_data.entries.data();
        spec_constant_info.dataSize = spec_constant_data.data.size();
        spec_constant_info.pData = spec_constant_data.data.data();
        shader_stage_create_info.pSpecializationInfo = &spec_constant_info;
    }
    else {
        shader_stage_create_info.pSpecializationInfo = nullptr;
    }

    VkPipelineLayoutCreateInfo pipeline_layout_create_info = {};
    pipeline_layout_create_info.sType =
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_create_info.pNext = nullptr;
    pipeline_layout_create_info.flags = 0;
    pipeline_layout_create_info.setLayoutCount =
        shader_module.descriptor_set_layouts().size();
    pipeline_layout_create_info.pSetLayouts =
        shader_module.descriptor_set_layouts().data();
    // TODO: support push constants
    pipeline_layout_create_info.pushConstantRangeCount = 0;
    pipeline_layout_create_info.pPushConstantRanges = nullptr;

    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    vkCreatePipelineLayout(device, &pipeline_layout_create_info,
                           /*pAllocator=*/nullptr, &pipeline_layout);

    VkComputePipelineCreateInfo pipeline_create_info = {};
    pipeline_create_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipeline_create_info.pNext = nullptr;
    pipeline_create_info.flags = 0;
    pipeline_create_info.stage = shader_stage_create_info;
    pipeline_create_info.layout = pipeline_layout;
    pipeline_create_info.basePipelineHandle = VK_NULL_HANDLE;
    pipeline_create_info.basePipelineIndex = 0;

    VkPipeline pipeline = VK_NULL_HANDLE;
    vkCreateComputePipelines(device, /*pipelineCache=*/VK_NULL_HANDLE,
                             /*createInfoCount=*/1, &pipeline_create_info,
                             /*pAllocator=*/nullptr, &pipeline);

    return new Pipeline(pipeline, device, pipeline_layout);
}

Pipeline::~Pipeline() { 
    vkDestroyPipeline(device_, pipeline_, /*pAllocator=*/nullptr);
    vkDestroyPipelineLayout(device_, pipeline_layout_, /*pAllocator=*/nullptr);
}

Pipeline::Pipeline(VkPipeline pipeline, VkDevice device, VkPipelineLayout layout)
    : pipeline_(pipeline),
      device_(device),
      pipeline_layout_(layout) 
      {}
}  // namespace vulkan
}  // namespace uvkc
