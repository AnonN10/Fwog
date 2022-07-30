#include "common/common.h"

#include <array>
#include <vector>
#include <tuple>

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#include <glm/gtx/transform.hpp>

#include <Fwog/BasicTypes.h>
#include <Fwog/Rendering.h>
#include <Fwog/DebugMarker.h>
#include <Fwog/Timer.h>
#include <Fwog/Pipeline.h>
#include <Fwog/Texture.h>
#include <Fwog/Buffer.h>
#include <Fwog/Shader.h>

////////////////////////////////////// Types
struct View
{
  glm::vec3 position{};
  float pitch{}; // pitch angle in radians
  float yaw{};   // yaw angle in radians

  glm::vec3 GetForwardDir() const
  {
    return glm::vec3
    {
      cos(pitch) * cos(yaw),
      sin(pitch),
      cos(pitch) * sin(yaw)
    };
  }

  glm::mat4 GetViewMatrix() const
  {
    return glm::lookAt(position, position + GetForwardDir(), glm::vec3(0, 1, 0));
  }

  void SetForwardDir(glm::vec3 dir)
  {
    assert(glm::abs(1.0f - glm::length(dir)) < 0.0001f);
    pitch = glm::asin(dir.y);
    yaw = glm::acos(dir.x / glm::cos(pitch));
    if (dir.x >= 0 && dir.z < 0)
      yaw *= -1;
  }
};

struct ObjectUniforms
{
  glm::mat4 model;
  glm::vec4 color;
};

struct Vertex
{
  glm::vec3 position;
  glm::vec3 normal;
  glm::vec2 uv;
};

struct GlobalUniforms
{
  glm::mat4 viewProj;
  glm::mat4 invViewProj;
};

struct ShadingUniforms
{
  glm::mat4 sunViewProj;
  glm::vec4 sunDir;
  glm::vec4 sunStrength;
};

struct alignas(16) RSMUniforms
{
  glm::mat4 sunViewProj;
  glm::mat4 invSunViewProj;
  glm::ivec2 targetDim;
  float rMax;
  uint32_t currentPass;
  uint32_t samples;
};

////////////////////////////////////// Globals
//constexpr int gWindowWidth = 1920;
//constexpr int gWindowHeight = 1080;
constexpr int gWindowWidth = 1280;
constexpr int gWindowHeight = 720;
float gPreviousCursorX = gWindowWidth / 2.0f;
float gPreviousCursorY = gWindowHeight / 2.0f;
float gCursorOffsetX = 0;
float gCursorOffsetY = 0;
float gSensitivity = 0.005f;

// scene parameters
uint32_t gRSMSamples = 400;
float gRMax = 0.08f;

constexpr int gShadowmapWidth = 1024;
constexpr int gShadowmapHeight = 1024;

std::array<Vertex, 24> gCubeVertices
{
  // front (+z)
  Vertex
  { { -0.5, -0.5, 0.5 }, { 0, 0, 1 }, { 0, 0 } },
  { {  0.5, -0.5, 0.5 }, { 0, 0, 1 }, { 1, 0 } },
  { {  0.5, 0.5,  0.5 }, { 0, 0, 1 }, { 1, 1 } },
  { { -0.5, 0.5,  0.5 }, { 0, 0, 1 }, { 0, 1 } },

  // back (-z)
  { { -0.5, 0.5,  -0.5 }, { 0, 0, -1 }, { 1, 1 } },
  { {  0.5, 0.5,  -0.5 }, { 0, 0, -1 }, { 0, 1 } },
  { {  0.5, -0.5, -0.5 }, { 0, 0, -1 }, { 0, 0 } },
  { { -0.5, -0.5, -0.5 }, { 0, 0, -1 }, { 1, 0 } },

  // left (-x)
  { { -0.5, -0.5,-0.5 }, { -1, 0, 0 }, { 0, 0 } },
  { { -0.5, -0.5, 0.5 }, { -1, 0, 0 }, { 1, 0 } },
  { { -0.5, 0.5,  0.5 }, { -1, 0, 0 }, { 1, 1 } },
  { { -0.5, 0.5, -0.5 }, { -1, 0, 0 }, { 0, 1 } },

  // right (+x)
  { { 0.5, 0.5,  -0.5 }, { 1, 0, 0 }, { 1, 1 } },
  { { 0.5, 0.5,   0.5 }, { 1, 0, 0 }, { 0, 1 } },
  { { 0.5, -0.5,  0.5 }, { 1, 0, 0 }, { 0, 0 } },
  { { 0.5, -0.5, -0.5 }, { 1, 0, 0 }, { 1, 0 } },

  // top (+y)
  { {-0.5, 0.5, 0.5 }, { 0, 1, 0 }, { 0, 0 } },
  { { 0.5, 0.5, 0.5 }, { 0, 1, 0 }, { 1, 0 } },
  { { 0.5, 0.5,-0.5 }, { 0, 1, 0 }, { 1, 1 } },
  { {-0.5, 0.5,-0.5 }, { 0, 1, 0 }, { 0, 1 } },

  // bottom (-y)
  { {-0.5, -0.5,-0.5 }, { 0, -1, 0 }, { 0, 0 } },
  { { 0.5, -0.5,-0.5 }, { 0, -1, 0 }, { 1, 0 } },
  { { 0.5, -0.5, 0.5 }, { 0, -1, 0 }, { 1, 1 } },
  { {-0.5, -0.5, 0.5 }, { 0, -1, 0 }, { 0, 1 } },
};

std::array<uint16_t, 36> gCubeIndices
{
  0, 1, 2,
  2, 3, 0,

  4, 5, 6,
  6, 7, 4,

  8, 9, 10,
  10, 11, 8,

  12, 13, 14,
  14, 15, 12,

  16, 17, 18,
  18, 19, 16,

  20, 21, 22,
  22, 23, 20,
};

std::array<Fwog::VertexInputBindingDescription, 3> GetSceneInputBindingDescs()
{
  Fwog::VertexInputBindingDescription descPos
  {
    .location = 0,
    .binding = 0,
    .format = Fwog::Format::R32G32B32_FLOAT,
    .offset = offsetof(Vertex, position),
  };
  Fwog::VertexInputBindingDescription descNormal
  {
    .location = 1,
    .binding = 0,
    .format = Fwog::Format::R32G32B32_FLOAT,
    .offset = offsetof(Vertex, normal),
  };
  Fwog::VertexInputBindingDescription descUV
  {
    .location = 2,
    .binding = 0,
    .format = Fwog::Format::R32G32_FLOAT,
    .offset = offsetof(Vertex, uv),
  };

  return { descPos, descNormal, descUV };
}

Fwog::GraphicsPipeline CreateScenePipeline()
{
  auto vertexShader = Fwog::Shader(
    Fwog::PipelineStage::VERTEX_SHADER,
    Utility::LoadFile("shaders/SceneDeferred.vert.glsl"));
  auto fragmentShader = Fwog::Shader(
    Fwog::PipelineStage::FRAGMENT_SHADER,
    Utility::LoadFile("shaders/SceneDeferred.frag.glsl"));

  auto pipeline = Fwog::CompileGraphicsPipeline(
    {
      .vertexShader = &vertexShader,
      .fragmentShader = &fragmentShader,
      .vertexInputState = { GetSceneInputBindingDescs() },
      .depthState = {.depthTestEnable = true, .depthWriteEnable = true }
    });

  return pipeline;
}

Fwog::GraphicsPipeline CreateShadowPipeline()
{
  auto vertexShader = Fwog::Shader(
    Fwog::PipelineStage::VERTEX_SHADER,
    Utility::LoadFile("shaders/SceneDeferred.vert.glsl"));
  auto fragmentShader = Fwog::Shader(
    Fwog::PipelineStage::FRAGMENT_SHADER,
    Utility::LoadFile("shaders/RSMScene.frag.glsl"));

  auto pipeline = Fwog::CompileGraphicsPipeline(
    {
      .vertexShader = &vertexShader,
      .fragmentShader = &fragmentShader,
      .vertexInputState = { GetSceneInputBindingDescs() },
      .rasterizationState =
      {
        .depthBiasEnable = true,
        .depthBiasConstantFactor = 0.0f,
        .depthBiasSlopeFactor = 2.0f,
      },
      .depthState = {.depthTestEnable = true, .depthWriteEnable = true }
    });

  return pipeline;
}

Fwog::GraphicsPipeline CreateShadingPipeline()
{
  auto vertexShader = Fwog::Shader(
    Fwog::PipelineStage::VERTEX_SHADER,
    Utility::LoadFile("shaders/FullScreenTri.vert.glsl"));
  auto fragmentShader = Fwog::Shader(
    Fwog::PipelineStage::FRAGMENT_SHADER,
    Utility::LoadFile("shaders/ShadeDeferred.frag.glsl"));

  auto pipeline = Fwog::CompileGraphicsPipeline(
    {
      .vertexShader = &vertexShader,
      .fragmentShader = &fragmentShader,
      .rasterizationState = { .cullMode = Fwog::CullMode::NONE },
    });

  return pipeline;
}

Fwog::GraphicsPipeline CreateDebugTexturePipeline()
{
  auto vertexShader = Fwog::Shader(
    Fwog::PipelineStage::VERTEX_SHADER,
    Utility::LoadFile("shaders/FullScreenTri.vert.glsl"));
  auto fragmentShader = Fwog::Shader(
    Fwog::PipelineStage::FRAGMENT_SHADER,
    Utility::LoadFile("shaders/Texture.frag.glsl"));

  auto pipeline = Fwog::CompileGraphicsPipeline(
    {
      .vertexShader = &vertexShader,
      .fragmentShader = &fragmentShader,
      .rasterizationState = {.cullMode = Fwog::CullMode::NONE },
    });

  return pipeline;
}

Fwog::ComputePipeline CreateRSMIndirectPipeline()
{
  auto shader = Fwog::Shader(
    Fwog::PipelineStage::COMPUTE_SHADER,
    Utility::LoadFile("shaders/RSMIndirect.comp.glsl"));

  auto pipeline = Fwog::CompileComputePipeline({ &shader });

  return pipeline;
}

void CursorPosCallback([[maybe_unused]] GLFWwindow* window, double currentCursorX, double currentCursorY)
{
  static bool firstFrame = true;
  if (firstFrame)
  {
    gPreviousCursorX = static_cast<float>(currentCursorX);
    gPreviousCursorY = static_cast<float>(currentCursorY);
    firstFrame = false;
  }

  gCursorOffsetX = static_cast<float>(currentCursorX) - gPreviousCursorX;
  gCursorOffsetY = gPreviousCursorY - static_cast<float>(currentCursorY);
  gPreviousCursorX = static_cast<float>(currentCursorX);
  gPreviousCursorY = static_cast<float>(currentCursorY);
}

void RenderScene()
{
  GLFWwindow* window = Utility::CreateWindow({
    .name = "Deferred Example",
    .maximize = false,
    .decorate = true,
    .width = gWindowWidth,
    .height = gWindowHeight });
  Utility::InitOpenGL();

  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
  glfwSetCursorPosCallback(window, CursorPosCallback);
  glEnable(GL_FRAMEBUFFER_SRGB);

  Fwog::Viewport mainViewport
  {
    .drawRect
    {
      .offset = { 0, 0 },
      .extent = { gWindowWidth, gWindowHeight }
    },
    .minDepth = 0.0f,
    .maxDepth = 1.0f,
  };

  Fwog::Viewport rsmViewport
  {
    .drawRect
    {
      .offset = { 0, 0 },
      .extent = { gShadowmapWidth, gShadowmapHeight }
    },
    .minDepth = 0.0f,
    .maxDepth = 1.0f,
  };

  Fwog::SwapchainRenderInfo swapchainRenderingInfo
  {
    .viewport = &mainViewport,
    .clearColorOnLoad = false,
    .clearColorValue = Fwog::ClearColorValue {.f = { .0f, .0f, .0f, 1.0f }},
    .clearDepthOnLoad = false,
    .clearStencilOnLoad = false,
  };

  // create gbuffer textures and render info
  auto gcolorTex = Fwog::CreateTexture2D({ gWindowWidth, gWindowHeight }, Fwog::Format::R8G8B8A8_UNORM);
  auto gnormalTex = Fwog::CreateTexture2D({ gWindowWidth, gWindowHeight }, Fwog::Format::R16G16B16_SNORM);
  auto gdepthTex = Fwog::CreateTexture2D({ gWindowWidth, gWindowHeight }, Fwog::Format::D32_UNORM);

  Fwog::RenderAttachment gcolorAttachment
  {
    .texture = &gcolorTex,
    .clearValue = Fwog::ClearValue{.color{.f{ .1f, .3f, .5f, 0.0f } } },
    .clearOnLoad = true
  };
  Fwog::RenderAttachment gnormalAttachment
  {
    .texture = &gnormalTex,
    .clearValue = Fwog::ClearValue{.color{.f{ 0, 0, 0, 0 } } },
    .clearOnLoad = false
  };
  Fwog::RenderAttachment gdepthAttachment
  {
    .texture = &gdepthTex,
    .clearValue = Fwog::ClearValue{.depthStencil{.depth = 1.0f } },
    .clearOnLoad = true
  };
  Fwog::RenderAttachment cgAttachments[] = { gcolorAttachment, gnormalAttachment };
  Fwog::RenderInfo gbufferRenderInfo
  {
    .viewport = &mainViewport,
    .colorAttachments = cgAttachments,
    .depthAttachment = &gdepthAttachment,
    .stencilAttachment = nullptr
  };

  // create RSM textures and render info
  auto rfluxTex = Fwog::CreateTexture2D({ gShadowmapWidth, gShadowmapHeight }, Fwog::Format::R11G11B10_FLOAT);
  auto rnormalTex = Fwog::CreateTexture2D({ gShadowmapWidth, gShadowmapHeight }, Fwog::Format::R16G16B16_SNORM);
  auto rdepthTex = Fwog::CreateTexture2D({ gShadowmapWidth, gShadowmapHeight }, Fwog::Format::D16_UNORM);

  Fwog::RenderAttachment rcolorAttachment
  {
    .texture = &rfluxTex,
    .clearValue = Fwog::ClearValue{.color{.f{ 0, 0, 0, 0 } } },
    .clearOnLoad = false
  };
  Fwog::RenderAttachment rnormalAttachment
  {
    .texture = &rnormalTex,
    .clearValue = Fwog::ClearValue{.color{.f{ 0, 0, 0, 0 } } },
    .clearOnLoad = false
  };
  Fwog::RenderAttachment rdepthAttachment
  {
    .texture = &rdepthTex,
    .clearValue = Fwog::ClearValue{.depthStencil{.depth = 1.0f } },
    .clearOnLoad = true
  };
  Fwog::RenderAttachment crAttachments[] = { rcolorAttachment, rnormalAttachment };
  Fwog::RenderInfo rsmRenderInfo
  {
    .viewport = &rsmViewport,
    .colorAttachments = crAttachments,
    .depthAttachment = &rdepthAttachment,
    .stencilAttachment = nullptr
  };

  auto indirectLightingTex = Fwog::CreateTexture2D({ gWindowWidth, gWindowHeight }, Fwog::Format::R16G16B16A16_FLOAT);
  
  auto view = glm::mat4(1);
  auto proj = glm::perspective(glm::radians(70.f), gWindowWidth / (float)gWindowHeight, 0.1f, 100.f);

  std::vector<ObjectUniforms> objectUniforms;
  // translation, scale, color tuples
  std::tuple<glm::vec3, glm::vec3, glm::vec3> objects[]{
    { { 0, .5, -1 },   { 3, 1, 1 },      { .5, .5, .5 } },
    { { -1, .5, 0 },   { 1, 1, 1 },      { .1, .1, .9 } },
    { { 1, .5, 0 },    { 1, 1, 1 },      { .1, .1, .9 } },
    { { 0, -.5, -.5 }, { 3, 1, 2 },      { .5, .5, .5 } },
    { { 0, 1.5, -.5 }, { 3, 1, 2 },      { .2, .7, .2 } },
    { { 0, .25, 0 },   { 0.25, .5, .25 }, { .5, .1, .1 } },
    //{ { -.25, .25, 0 },   { .01, .5, .7 }, { .5, .1, .1 } },
    //{ { .25, .25, 0 },   { .01, .5, .7 }, { .5, .1, .1 } },
    //{ { 0, .25, -.25 },   { .7, .5, .01 }, { .5, .1, .1 } },
    //{ { 0, .25, .25 },   { .7, .5, .01 }, { .5, .1, .1 } },
  };
  for (const auto& [translation, scale, color] : objects)
  {
    glm::mat4 model{ 1 };
    model = glm::translate(model, translation);
    model = glm::scale(model, scale);
    objectUniforms.push_back({ model, glm::vec4{ color, 0.0f } });
  }

  ShadingUniforms shadingUniforms
  {
    .sunDir = glm::normalize(glm::vec4{ -.1, -.3, -.6, 0 }),
    .sunStrength = glm::vec4{ 2, 2, 2, 0 },
  };

  RSMUniforms rsmUniforms
  {
    .targetDim = { gWindowWidth, gWindowHeight },
    .rMax = gRMax,
    .samples = gRSMSamples,
  };

  GlobalUniforms globalUniforms{};

  auto vertexBuffer = Fwog::Buffer(gCubeVertices);
  auto indexBuffer = Fwog::Buffer(gCubeIndices);
  auto objectBuffer = Fwog::Buffer(std::span(objectUniforms), Fwog::BufferFlag::DYNAMIC_STORAGE);
  auto globalUniformsBuffer = Fwog::Buffer(sizeof(globalUniforms), Fwog::BufferFlag::DYNAMIC_STORAGE);
  auto shadingUniformsBuffer = Fwog::Buffer(shadingUniforms, Fwog::BufferFlag::DYNAMIC_STORAGE);
  auto rsmUniformBuffer = Fwog::Buffer(rsmUniforms, Fwog::BufferFlag::DYNAMIC_STORAGE);

  Fwog::SamplerState ss;
  ss.minFilter = Fwog::Filter::NEAREST;
  ss.magFilter = Fwog::Filter::NEAREST;
  ss.addressModeU = Fwog::AddressMode::REPEAT;
  ss.addressModeV = Fwog::AddressMode::REPEAT;
  auto nearestSampler = Fwog::Sampler(ss);

  ss.minFilter = Fwog::Filter::LINEAR;
  ss.magFilter = Fwog::Filter::LINEAR;
  ss.borderColor = Fwog::BorderColor::FLOAT_TRANSPARENT_BLACK;
  ss.addressModeU = Fwog::AddressMode::CLAMP_TO_BORDER;
  ss.addressModeV = Fwog::AddressMode::CLAMP_TO_BORDER;
  auto rsmColorSampler = Fwog::Sampler(ss);

  ss.minFilter = Fwog::Filter::NEAREST;
  ss.magFilter = Fwog::Filter::NEAREST;
  ss.borderColor = Fwog::BorderColor::FLOAT_TRANSPARENT_BLACK;
  ss.addressModeU = Fwog::AddressMode::CLAMP_TO_BORDER;
  ss.addressModeV = Fwog::AddressMode::CLAMP_TO_BORDER;
  auto rsmDepthSampler = Fwog::Sampler(ss);

  ss.compareEnable = true;
  ss.compareOp = Fwog::CompareOp::LESS;
  ss.minFilter = Fwog::Filter::LINEAR;
  ss.magFilter = Fwog::Filter::LINEAR;
  auto rsmShadowSampler = Fwog::Sampler(ss);

  Fwog::GraphicsPipeline scenePipeline = CreateScenePipeline();
  Fwog::GraphicsPipeline rsmScenePipeline = CreateShadowPipeline();
  Fwog::GraphicsPipeline shadingPipeline = CreateShadingPipeline();
  Fwog::ComputePipeline rsmIndirectPipeline = CreateRSMIndirectPipeline();
  Fwog::GraphicsPipeline debugTexturePipeline = CreateDebugTexturePipeline();

  View camera;
  camera.position = { 0, .5, 1 };
  camera.yaw = -glm::half_pi<float>();

  float prevFrame = static_cast<float>(glfwGetTime());
  while (!glfwWindowShouldClose(window))
  {
    float curFrame = static_cast<float>(glfwGetTime());
    float dt = curFrame - prevFrame;
    prevFrame = curFrame;

    gCursorOffsetX = 0;
    gCursorOffsetY = 0;
    glfwPollEvents();
    if (glfwGetKey(window, GLFW_KEY_ESCAPE))
    {
      glfwSetWindowShouldClose(window, true);
    }

    const glm::vec3 forward = camera.GetForwardDir();
    const glm::vec3 up = { 0, 1, 0 };
    const glm::vec3 right = glm::normalize(glm::cross(forward, up));
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
      camera.position += forward * dt;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
      camera.position -= forward * dt;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
      camera.position += right * dt;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
      camera.position -= right * dt;
    camera.yaw += gCursorOffsetX * gSensitivity;
    camera.pitch += gCursorOffsetY * gSensitivity;
    camera.pitch = glm::clamp(camera.pitch, -glm::half_pi<float>() + 1e-4f, glm::half_pi<float>() - 1e-4f);

    //objectUniforms[5].model = glm::rotate(glm::mat4(1), dt, {0, 1, 0}) * objectUniforms[5].model;
    //objectBuffer->SubData(std::span(objectUniforms), 0);

    if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS)
    {
      rsmUniforms.rMax -= .15f * dt;
      printf("rMax: %f\n", rsmUniforms.rMax);
    }
    if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS)
    {
      rsmUniforms.rMax += .15f * dt;
      printf("rMax: %f\n", rsmUniforms.rMax);
    }
    rsmUniforms.rMax = glm::clamp(rsmUniforms.rMax, 0.02f, 0.3f);

    if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS)
    {
      shadingUniforms.sunDir = glm::rotate(glm::quarter_pi<float>() * dt, glm::vec3{ 1, 0, 0 }) * shadingUniforms.sunDir;
    }
    if (glfwGetKey(window, GLFW_KEY_4) == GLFW_PRESS)
    {
      shadingUniforms.sunDir = glm::rotate(glm::quarter_pi<float>() * dt, glm::vec3{ -1, 0, 0 }) * shadingUniforms.sunDir;
    }

    glm::mat4 viewProj = proj * camera.GetViewMatrix();
    globalUniformsBuffer.SubData(viewProj, 0);

    glm::vec3 eye = glm::vec3{ shadingUniforms.sunDir * -5.f };
    float eyeWidth = 2.5f;
    //shadingUniforms.viewPos = glm::vec4(camera.position, 0);
    shadingUniforms.sunViewProj =
      glm::ortho(-eyeWidth, eyeWidth, -eyeWidth, eyeWidth, .1f, 10.f) *
      glm::lookAt(eye, glm::vec3(0), glm::vec3{ 0, 1, 0 });
    shadingUniformsBuffer.SubData(shadingUniforms, 0);

    // geometry buffer pass
    Fwog::BeginRendering(gbufferRenderInfo);
    {
      Fwog::ScopedDebugMarker marker("Geometry");
      Fwog::Cmd::BindGraphicsPipeline(scenePipeline);
      Fwog::Cmd::BindVertexBuffer(0, vertexBuffer, 0, sizeof(Vertex));
      Fwog::Cmd::BindIndexBuffer(indexBuffer, Fwog::IndexType::UNSIGNED_SHORT);
      Fwog::Cmd::BindUniformBuffer(0, globalUniformsBuffer, 0, globalUniformsBuffer.Size());
      Fwog::Cmd::BindStorageBuffer(1, objectBuffer, 0, objectBuffer.Size());
      Fwog::Cmd::DrawIndexed(static_cast<uint32_t>(gCubeIndices.size()), static_cast<uint32_t>(objectUniforms.size()), 0, 0, 0);
    }
    Fwog::EndRendering();

    globalUniformsBuffer.SubData(shadingUniforms.sunViewProj, 0);

    // shadow map (RSM) scene pass
    Fwog::BeginRendering(rsmRenderInfo);
    {
      Fwog::ScopedDebugMarker marker("RSM Scene");
      Fwog::Cmd::BindGraphicsPipeline(rsmScenePipeline);
      Fwog::Cmd::BindVertexBuffer(0, vertexBuffer, 0, sizeof(Vertex));
      Fwog::Cmd::BindIndexBuffer(indexBuffer, Fwog::IndexType::UNSIGNED_SHORT);
      Fwog::Cmd::BindUniformBuffer(0, globalUniformsBuffer, 0, globalUniformsBuffer.Size());
      Fwog::Cmd::BindUniformBuffer(1, shadingUniformsBuffer, 0, shadingUniformsBuffer.Size());
      Fwog::Cmd::BindStorageBuffer(1, objectBuffer, 0, objectBuffer.Size());
      Fwog::Cmd::DrawIndexed(static_cast<uint32_t>(gCubeIndices.size()), static_cast<uint32_t>(objectUniforms.size()), 0, 0, 0);
    }
    Fwog::EndRendering();

    globalUniformsBuffer.SubData(viewProj, 0);
    globalUniformsBuffer.SubData(glm::inverse(viewProj), sizeof(glm::mat4));

    rsmUniforms.sunViewProj = shadingUniforms.sunViewProj;
    rsmUniforms.invSunViewProj = glm::inverse(rsmUniforms.sunViewProj);
    rsmUniformBuffer.SubData(rsmUniforms, 0);

    // RSM indirect illumination calculation pass
    Fwog::BeginCompute();
    {
      // uncomment to benchmark
      //static Fwog::TimerQueryAsync timer(5);
      //if (auto t = timer.PopTimestamp())
      //{
      //  printf("Indirect Illumination: %f ms\n", *t / 10e5);
      //}
      //Fwog::TimerScoped scopedTimer(timer);
      Fwog::ScopedDebugMarker marker("Indirect Illumination");
      Fwog::Cmd::BindComputePipeline(rsmIndirectPipeline);
      Fwog::Cmd::BindSampledImage(0, indirectLightingTex, nearestSampler);
      Fwog::Cmd::BindSampledImage(1, gcolorTex, nearestSampler);
      Fwog::Cmd::BindSampledImage(2, gnormalTex, nearestSampler);
      Fwog::Cmd::BindSampledImage(3, gdepthTex, nearestSampler);
      Fwog::Cmd::BindSampledImage(4, rfluxTex, nearestSampler);
      Fwog::Cmd::BindSampledImage(5, rnormalTex, nearestSampler);
      Fwog::Cmd::BindSampledImage(6, rdepthTex, nearestSampler);
      Fwog::Cmd::BindUniformBuffer(0, globalUniformsBuffer, 0, globalUniformsBuffer.Size());
      Fwog::Cmd::BindUniformBuffer(1, rsmUniformBuffer, 0, rsmUniformBuffer.Size());
      Fwog::Cmd::BindImage(0, indirectLightingTex, 0);

      const int localSize = 8;
      const int numGroupsX = (rsmUniforms.targetDim.x / 2 + localSize - 1) / localSize;
      const int numGroupsY = (rsmUniforms.targetDim.y / 2 + localSize - 1) / localSize;

      uint32_t currentPass = 0;
      rsmUniformBuffer.SubData(currentPass, offsetof(RSMUniforms, currentPass));
      Fwog::Cmd::Dispatch(numGroupsX, numGroupsY, 1);
      Fwog::Cmd::MemoryBarrier(Fwog::MemoryBarrierAccessBit::TEXTURE_FETCH_BIT);

      currentPass = 1;
      rsmUniformBuffer.SubData(currentPass, offsetof(RSMUniforms, currentPass));
      Fwog::Cmd::Dispatch(numGroupsX, numGroupsY, 1);
      Fwog::Cmd::MemoryBarrier(Fwog::MemoryBarrierAccessBit::TEXTURE_FETCH_BIT);

      currentPass = 2;
      rsmUniformBuffer.SubData(currentPass, offsetof(RSMUniforms, currentPass));
      Fwog::Cmd::Dispatch(numGroupsX, numGroupsY, 1);
      Fwog::Cmd::MemoryBarrier(Fwog::MemoryBarrierAccessBit::TEXTURE_FETCH_BIT);

      currentPass = 3;
      rsmUniformBuffer.SubData(currentPass, offsetof(RSMUniforms, currentPass));
      Fwog::Cmd::Dispatch(numGroupsX, numGroupsY, 1);
      Fwog::Cmd::MemoryBarrier(Fwog::MemoryBarrierAccessBit::TEXTURE_FETCH_BIT);
    }
    Fwog::EndCompute();

    // shading pass (full screen tri)
    Fwog::BeginSwapchainRendering(swapchainRenderingInfo);
    {
      Fwog::ScopedDebugMarker marker("Shading");
      Fwog::Cmd::BindGraphicsPipeline(shadingPipeline);
      Fwog::Cmd::BindSampledImage(0, gcolorTex, nearestSampler);
      Fwog::Cmd::BindSampledImage(1, gnormalTex, nearestSampler);
      Fwog::Cmd::BindSampledImage(2, gdepthTex, nearestSampler);
      Fwog::Cmd::BindSampledImage(3, indirectLightingTex, nearestSampler);
      Fwog::Cmd::BindSampledImage(4, rdepthTex, rsmShadowSampler);
      Fwog::Cmd::BindUniformBuffer(0, globalUniformsBuffer, 0, globalUniformsBuffer.Size());
      Fwog::Cmd::BindUniformBuffer(1, shadingUniformsBuffer, 0, shadingUniformsBuffer.Size());
      Fwog::Cmd::Draw(3, 1, 0, 0);

      Fwog::Texture* tex{};
      if (glfwGetKey(window, GLFW_KEY_F1) == GLFW_PRESS)
        tex = &gcolorTex; 
      if (glfwGetKey(window, GLFW_KEY_F2) == GLFW_PRESS)
        tex = &gnormalTex;
      if (glfwGetKey(window, GLFW_KEY_F3) == GLFW_PRESS)
        tex = &gdepthTex;
      if (glfwGetKey(window, GLFW_KEY_F4) == GLFW_PRESS)
        tex = &indirectLightingTex;
      if (tex)
      {
        Fwog::Cmd::BindGraphicsPipeline(debugTexturePipeline);
        Fwog::Cmd::BindSampledImage(0, *tex, nearestSampler);
        Fwog::Cmd::Draw(3, 1, 0, 0);
      }
    }
    Fwog::EndRendering();

    glfwSwapBuffers(window);
  }

  glfwTerminate();
}

int main()
{
  try
  {
    RenderScene();
  }
  catch (std::exception& e)
  {
    printf("Error: %s", e.what());
    throw;
  }
  catch (...)
  {
    printf("Unknown error");
    throw;
  }

  return 0;
}