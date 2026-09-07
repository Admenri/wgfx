// Copyright 2026 Admenri.
// Use of this source code is governed by a MIT-style license that can be
// found in the LICENSE file.

#include "gfx/gfx_render_pipeline.h"

#include "gfx/gfx_auto_layout.h"
#include "gfx/common/vulkan_conversions.h"
#include "gfx/gfx_bind_group_layout.h"
#include "gfx/gfx_device.h"
#include "gfx/gfx_pipeline_layout.h"
#include "gfx/gfx_shader_module.h"

namespace gfx {

namespace {

VkRenderPass CreateRenderPassForPipeline(Device* device,
                                         const WGPURenderPipelineDescriptor* d) {
  std::vector<VkAttachmentDescription> attachments;
  std::vector<VkAttachmentReference> color_refs;
  VkAttachmentReference depth_ref = {};

  size_t target_count = d->fragment ? d->fragment->targetCount : 0;
  const WGPUColorTargetState* targets = d->fragment ? d->fragment->targets
                                                    : nullptr;
  for (size_t i = 0; i < target_count; ++i) {
    if (targets[i].format == WGPUTextureFormat_Undefined) {
      color_refs.push_back({VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_GENERAL});
      continue;
    }
    VkAttachmentDescription attachment = {};
    attachment.format = GetFormatInfo(targets[i].format).vk_format;
    attachment.samples = static_cast<VkSampleCountFlagBits>(
        d->multisample.count ? d->multisample.count : 1);
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachment.finalLayout = VK_IMAGE_LAYOUT_GENERAL;
    color_refs.push_back({static_cast<uint32_t>(attachments.size()),
                          VK_IMAGE_LAYOUT_GENERAL});
    attachments.push_back(attachment);
  }

  bool has_depth = d->depthStencil &&
                   d->depthStencil->format != WGPUTextureFormat_Undefined;
  if (has_depth) {
    FormatInfo info = GetFormatInfo(d->depthStencil->format);
    VkAttachmentDescription attachment = {};
    attachment.format = info.vk_format;
    attachment.samples = static_cast<VkSampleCountFlagBits>(
        d->multisample.count ? d->multisample.count : 1);
    attachment.loadOp = info.depth ? VK_ATTACHMENT_LOAD_OP_CLEAR
                                   : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp =
        info.stencil ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = info.stencil
                                    ? VK_ATTACHMENT_STORE_OP_STORE
                                    : VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachment.finalLayout = VK_IMAGE_LAYOUT_GENERAL;
    depth_ref = {static_cast<uint32_t>(attachments.size()),
                 VK_IMAGE_LAYOUT_GENERAL};
    attachments.push_back(attachment);
  }

  VkSubpassDescription subpass = {};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = static_cast<uint32_t>(color_refs.size());
  subpass.pColorAttachments = color_refs.data();
  subpass.pDepthStencilAttachment = has_depth ? &depth_ref : nullptr;

  VkRenderPassCreateInfo create_info = {};
  create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  create_info.attachmentCount = static_cast<uint32_t>(attachments.size());
  create_info.pAttachments = attachments.data();
  create_info.subpassCount = 1;
  create_info.pSubpasses = &subpass;

  VkRenderPass render_pass = VK_NULL_HANDLE;
  vkCreateRenderPass(device->GetVkDevice(), &create_info, nullptr,
                     &render_pass);
  return render_pass;
}

VkPipelineShaderStageCreateInfo MakeStage(
    Device* device, gfx::ShaderModule* module,
    const WGPUStringView& entry_point, VkShaderStageFlagBits stage) {
  VkShaderModuleCreateInfo module_info = {};
  module_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  module_info.codeSize = module->GetCodeWordCount() * sizeof(uint32_t);
  module_info.pCode = module->GetCode();

  VkShaderModule shader_module = VK_NULL_HANDLE;
  vkCreateShaderModule(device->GetVkDevice(), &module_info, nullptr,
                       &shader_module);

  // The module object is owned by the pipeline creation call; destroy it
  // after pipeline creation (handled by the caller via the returned info).
  VkPipelineShaderStageCreateInfo stage_info = {};
  stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stage_info.stage = stage;
  stage_info.module = shader_module;
  static thread_local std::string entry_name;
  std::string_view resolved = gfx::FromWGPUStringView(entry_point);
  entry_name.assign(resolved.data(), resolved.size());
  if (entry_name.empty())
    entry_name = "main";
  stage_info.pName = entry_name.c_str();
  stage_info.pSpecializationInfo = nullptr;
  return stage_info;
}

}  // namespace

RenderPipeline::RenderPipeline(
    RefPtr<Device> device, WGPURenderPipelineDescriptor const * descriptor)
    : device_(std::move(device)) {
  // Pipeline layout: user provided, or built from SPIR-V reflection.
  if (descriptor->layout) {
    layout_ = RefPtr<PipelineLayout>(
        static_cast<gfx::PipelineLayout*>(descriptor->layout));
  } else {
    std::vector<gfx::ShaderModule*> modules;
    modules.push_back(static_cast<gfx::ShaderModule*>(descriptor->vertex.module));
    if (descriptor->fragment)
      modules.push_back(
          static_cast<gfx::ShaderModule*>(descriptor->fragment->module));
      layout_ = BuildAutoPipelineLayout(device_.get(), modules);
    }

  VkRenderPass pipeline_render_pass =
      CreateRenderPassForPipeline(device_.get(), descriptor);

  // Vertex input state.
  std::vector<VkVertexInputBindingDescription> binding_descriptions;
  std::vector<VkVertexInputAttributeDescription> attribute_descriptions;
  for (size_t i = 0; i < descriptor->vertex.bufferCount; ++i) {
    const WGPUVertexBufferLayout& layout = descriptor->vertex.buffers[i];
    VkVertexInputBindingDescription binding = {};
    binding.binding = static_cast<uint32_t>(i);
    binding.stride = static_cast<uint32_t>(layout.arrayStride);
    binding.inputRate = layout.stepMode == WGPUVertexStepMode_Instance
                            ? VK_VERTEX_INPUT_RATE_INSTANCE
                            : VK_VERTEX_INPUT_RATE_VERTEX;
    binding_descriptions.push_back(binding);
    for (size_t j = 0; j < layout.attributeCount; ++j) {
      const WGPUVertexAttribute& attribute = layout.attributes[j];
      VkVertexInputAttributeDescription vk_attribute = {};
      vk_attribute.location = attribute.shaderLocation;
      vk_attribute.binding = binding.binding;
      vk_attribute.format = GetVertexFormat(attribute.format);
      vk_attribute.offset = static_cast<uint32_t>(attribute.offset);
      attribute_descriptions.push_back(vk_attribute);
    }
  }

  VkPipelineVertexInputStateCreateInfo vertex_input = {};
  vertex_input.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertex_input.vertexBindingDescriptionCount =
      static_cast<uint32_t>(binding_descriptions.size());
  vertex_input.pVertexBindingDescriptions = binding_descriptions.data();
  vertex_input.vertexAttributeDescriptionCount =
      static_cast<uint32_t>(attribute_descriptions.size());
  vertex_input.pVertexAttributeDescriptions = attribute_descriptions.data();

  VkPipelineInputAssemblyStateCreateInfo input_assembly = {};
  input_assembly.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  input_assembly.topology =
      GetPrimitiveTopology(descriptor->primitive.topology);
  // stripIndexFormat != undefined enables primitive restart for strips.
  input_assembly.primitiveRestartEnable =
      descriptor->primitive.stripIndexFormat != WGPUIndexFormat_Undefined;

  VkPipelineViewportStateCreateInfo viewport_state = {};
  viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewport_state.viewportCount = 1;
  viewport_state.scissorCount = 1;

  VkPipelineRasterizationStateCreateInfo rasterization = {};
  rasterization.sType =
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  const WGPUDepthStencilState* ds = descriptor->depthStencil;
  // VK_EXT_depth_clip_enable disables far-plane clipping for
  // WGPUFeatureName_DepthClipControl (primitive.unclippedDepth).
  VkPipelineRasterizationDepthClipStateCreateInfoEXT depth_clip_state = {};
  if (descriptor->primitive.unclippedDepth &&
      device_->IsFeatureEnabled(WGPUFeatureName_DepthClipControl)) {
    depth_clip_state.sType =
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_DEPTH_CLIP_STATE_CREATE_INFO_EXT;
    depth_clip_state.depthClipEnable = VK_FALSE;
    rasterization.pNext = &depth_clip_state;
  }
  rasterization.polygonMode = VK_POLYGON_MODE_FILL;
  rasterization.cullMode = GetCullMode(descriptor->primitive.cullMode);
  rasterization.frontFace = GetFrontFace(descriptor->primitive.frontFace);
  rasterization.lineWidth = 1.0f;
  if (ds) {
    rasterization.depthBiasEnable =
        (ds->depthBias != 0 || ds->depthBiasSlopeScale != 0.0f);
    rasterization.depthBiasConstantFactor = static_cast<float>(ds->depthBias);
    rasterization.depthBiasClamp = ds->depthBiasClamp;
    rasterization.depthBiasSlopeFactor = ds->depthBiasSlopeScale;
  }

  VkPipelineMultisampleStateCreateInfo multisample = {};
  multisample.sType =
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisample.rasterizationSamples = static_cast<VkSampleCountFlagBits>(
      descriptor->multisample.count ? descriptor->multisample.count : 1);
  multisample.alphaToCoverageEnable =
      descriptor->multisample.alphaToCoverageEnabled;

  VkPipelineDepthStencilStateCreateInfo depth_stencil = {};
  depth_stencil.sType =
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  if (ds) {
    depth_stencil.depthTestEnable = ds->depthWriteEnabled == WGPUOptionalBool_True ||
                                    ds->depthCompare != WGPUCompareFunction_Always;
    depth_stencil.depthWriteEnable =
        ds->depthWriteEnabled == WGPUOptionalBool_True;
    depth_stencil.depthCompareOp = GetCompareOp(ds->depthCompare);
    depth_stencil.stencilTestEnable =
        ds->stencilFront.compare != WGPUCompareFunction_Undefined ||
        ds->stencilBack.compare != WGPUCompareFunction_Undefined;
    depth_stencil.front.failOp = GetStencilOp(ds->stencilFront.failOp);
    depth_stencil.front.depthFailOp = GetStencilOp(ds->stencilFront.depthFailOp);
    depth_stencil.front.passOp = GetStencilOp(ds->stencilFront.passOp);
    depth_stencil.front.compareOp = GetCompareOp(ds->stencilFront.compare);
    depth_stencil.front.compareMask = ds->stencilReadMask;
    depth_stencil.front.writeMask = ds->stencilWriteMask;
    depth_stencil.front.reference = 0;
    depth_stencil.back.failOp = GetStencilOp(ds->stencilBack.failOp);
    depth_stencil.back.depthFailOp = GetStencilOp(ds->stencilBack.depthFailOp);
    depth_stencil.back.passOp = GetStencilOp(ds->stencilBack.passOp);
    depth_stencil.back.compareOp = GetCompareOp(ds->stencilBack.compare);
    depth_stencil.back.compareMask = ds->stencilReadMask;
    depth_stencil.back.writeMask = ds->stencilWriteMask;
    depth_stencil.back.reference = 0;
    depth_stencil.minDepthBounds = 0.0f;
    depth_stencil.maxDepthBounds = 1.0f;
  }

  // Blend state per color target.
  std::vector<VkPipelineColorBlendAttachmentState> blend_states;
  size_t blend_target_count = descriptor->fragment
                                  ? descriptor->fragment->targetCount
                                  : 0;
  blend_states.reserve(blend_target_count);
  for (size_t i = 0; i < blend_target_count; ++i) {
    const WGPUColorTargetState& target = descriptor->fragment->targets[i];
    VkPipelineColorBlendAttachmentState state = {};
    state.colorWriteMask = target.writeMask;
    if (target.blend) {
      state.blendEnable = VK_TRUE;
      state.srcColorBlendFactor = GetBlendFactor(target.blend->color.srcFactor);
      state.dstColorBlendFactor = GetBlendFactor(target.blend->color.dstFactor);
      state.colorBlendOp = GetBlendOp(target.blend->color.operation);
      state.srcAlphaBlendFactor = GetBlendFactor(target.blend->alpha.srcFactor);
      state.dstAlphaBlendFactor = GetBlendFactor(target.blend->alpha.dstFactor);
      state.alphaBlendOp = GetBlendOp(target.blend->alpha.operation);
    }
    blend_states.push_back(state);
  }
  VkPipelineColorBlendStateCreateInfo color_blend = {};
  color_blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  color_blend.attachmentCount = static_cast<uint32_t>(blend_states.size());
  color_blend.pAttachments = blend_states.data();

  VkDynamicState dynamic_states[] = {VK_DYNAMIC_STATE_VIEWPORT,
                                     VK_DYNAMIC_STATE_SCISSOR,
                                     VK_DYNAMIC_STATE_BLEND_CONSTANTS,
                                     VK_DYNAMIC_STATE_STENCIL_REFERENCE};
  VkPipelineDynamicStateCreateInfo dynamic_state = {};
  dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamic_state.dynamicStateCount = 4;
  dynamic_state.pDynamicStates = dynamic_states;

  std::vector<VkPipelineShaderStageCreateInfo> stages;
  VkShaderModule vertex_module = VK_NULL_HANDLE;
  VkShaderModule fragment_module = VK_NULL_HANDLE;
  {
    stages.push_back(MakeStage(device_.get(),
                               static_cast<gfx::ShaderModule*>(
                                   descriptor->vertex.module),
                               descriptor->vertex.entryPoint,
                               VK_SHADER_STAGE_VERTEX_BIT));
    vertex_module = stages.back().module;
    if (descriptor->fragment) {
      stages.push_back(MakeStage(device_.get(),
                                 static_cast<gfx::ShaderModule*>(
                                     descriptor->fragment->module),
                                 descriptor->fragment->entryPoint,
                                 VK_SHADER_STAGE_FRAGMENT_BIT));
      fragment_module = stages.back().module;
    }
  }

  VkGraphicsPipelineCreateInfo create_info = {};
  create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  create_info.stageCount = static_cast<uint32_t>(stages.size());
  create_info.pStages = stages.data();
  create_info.pVertexInputState = &vertex_input;
  create_info.pInputAssemblyState = &input_assembly;
  create_info.pViewportState = &viewport_state;
  create_info.pRasterizationState = &rasterization;
  create_info.pMultisampleState = &multisample;
  create_info.pDepthStencilState = &depth_stencil;
  create_info.pColorBlendState = &color_blend;
  create_info.pDynamicState = &dynamic_state;
  create_info.layout = layout_->GetVkPipelineLayout();
  create_info.renderPass = pipeline_render_pass;
  create_info.subpass = 0;

  VkResult pipeline_result = vkCreateGraphicsPipelines(
      device_->GetVkDevice(), VK_NULL_HANDLE, 1, &create_info, nullptr,
      &pipeline_);
  if (pipeline_result != VK_SUCCESS || !pipeline_) {
    // Without validation layers a failed pipeline is fatal to record.
    pipeline_ = VK_NULL_HANDLE;
  }

  if (vertex_module)
    vkDestroyShaderModule(device_->GetVkDevice(), vertex_module, nullptr);
  if (fragment_module)
    vkDestroyShaderModule(device_->GetVkDevice(), fragment_module, nullptr);
  vkDestroyRenderPass(device_->GetVkDevice(), pipeline_render_pass, nullptr);
}

RenderPipeline::~RenderPipeline() {
  if (pipeline_)
    vkDestroyPipeline(device_->GetVkDevice(), pipeline_, nullptr);
}

VkPipelineLayout RenderPipeline::GetVkPipelineLayout() const {
  return layout_->GetVkPipelineLayout();
}

VkShaderStageFlags RenderPipeline::GetImmediateStages() const {
  return layout_->GetImmediateStages();
}

BindGroupLayout* RenderPipeline::GetBindGroupLayoutObject(
    uint32_t group_index) const {
  return layout_->GetBindGroupLayouts()[group_index].get();
}

gfx::BindGroupLayout* RenderPipeline::GetBindGroupLayout(uint32_t groupIndex) {
  BindGroupLayout* layout = GetBindGroupLayoutObject(groupIndex);
  layout->AddRef();
  return layout;
}

void RenderPipeline::SetLabel(WGPUStringView label) {
  device_->SetObjectLabel(reinterpret_cast<uint64_t>(pipeline_),
                          VK_OBJECT_TYPE_PIPELINE, label);
}

}  // namespace gfx
