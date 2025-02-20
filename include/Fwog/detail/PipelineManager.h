#pragma once
#include <Fwog/Pipeline.h>
#include <memory>
#include <string>
#include <vector>

namespace Fwog::detail
{
  // owning versions of pipeline info structs so we don't lose references
  struct VertexInputStateOwning
  {
    std::vector<VertexInputBindingDescription> vertexBindingDescriptions;
  };

  struct ColorBlendStateOwning
  {
    bool logicOpEnable;
    LogicOp logicOp;
    std::vector<ColorBlendAttachmentState> attachments;
    float blendConstants[4];
  };

  struct GraphicsPipelineInfoOwning
  {
    std::string name;
    InputAssemblyState inputAssemblyState;
    VertexInputStateOwning vertexInputState;
    RasterizationState rasterizationState;
    DepthState depthState;
    StencilState stencilState;
    ColorBlendStateOwning colorBlendState;
  };

  struct ComputePipelineInfoOwning
  {
    std::string name;
  };

  uint64_t CompileGraphicsPipelineInternal(const GraphicsPipelineInfo& info);
  std::shared_ptr<const GraphicsPipelineInfoOwning> GetGraphicsPipelineInternal(uint64_t pipeline);
  void DestroyGraphicsPipelineInternal(uint64_t pipeline);

  uint64_t CompileComputePipelineInternal(const ComputePipelineInfo& info);
  std::shared_ptr<const ComputePipelineInfoOwning> GetComputePipelineInternal(uint64_t pipeline);
  void DestroyComputePipelineInternal(uint64_t pipeline);
} // namespace Fwog::detail