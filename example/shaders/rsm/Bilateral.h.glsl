#include "Kernels.h.glsl"
#include "Common.h.glsl"

// Input
layout(binding = 0) uniform sampler2D s_illuminance;
layout(binding = 1) uniform sampler2D s_gBufferNormal;
layout(binding = 2) uniform sampler2D s_gBufferDepth;
layout(binding = 3) uniform sampler2D s_moments;
layout(binding = 4) uniform usampler2D s_historyLength;

layout(binding = 0, std140) uniform FilterUniforms
{
  mat4 proj;
  mat4 invViewProj;
  vec3 viewPos;
  float stepWidth;
  ivec2 targetDim;
  ivec2 direction; // either (1, 0) or (0, 1)
  float phiLuminance;
  float phiNormal;
  float phiDepth;
}uniforms;

// Output
layout(binding = 0) uniform writeonly restrict image2D i_filteredIlluminance;

void AddFilterContribution(inout vec3 accumIlluminance,
                           inout float accumWeight,
                           vec3 cColor,
                           float cLuminance,
                           vec3 cNormal,
                           float cDepth,
                           float variance,
                           vec3 rayDir,
                           ivec2 baseOffset,
                           ivec2 offset,
                           ivec2 kernelStep,
                           float kernelWeight,
                           ivec2 id,
                           ivec2 gid)
{
  vec3 oColor = texelFetch(s_illuminance, id, 0).rgb;
  float oLuminance = Luminance(oColor);
  vec3 oNormal = texelFetch(s_gBufferNormal, id, 0).xyz;
  float oDepth = texelFetch(s_gBufferDepth, id, 0).x;
  float phiDepth = offset == ivec2(0) ? 1.0 : length(vec2(baseOffset));
  phiDepth *= uniforms.phiDepth;

  float normalWeight = NormalWeight(oNormal, cNormal, uniforms.phiNormal);
  float depthWeight = DepthWeight(oDepth, cDepth, cNormal, rayDir, uniforms.proj, phiDepth);
//variance = 1;
  float luminanceWeight = LuminanceWeight(oLuminance, cLuminance, variance, uniforms.phiLuminance);
luminanceWeight = 1; // Disable luminance weight
  float weight = normalWeight * depthWeight * luminanceWeight;
  accumIlluminance += oColor * weight * kernelWeight;
  accumWeight += weight * kernelWeight;
}

float ComputeVarianceCenter(ivec2 ipos)
{
  float sum = 0.f;

  const float kernel3[2][2] = {
    { 1.0 / 4.0, 1.0 / 8.0  },
    { 1.0 / 8.0, 1.0 / 16.0 }
  };

  const int radius = 1;
  for (int yy = -radius; yy <= radius; yy++)
  {
    for (int xx = -radius; xx <= radius; xx++)
    {
      const ivec2 p = ipos + ivec2(xx, yy);
      const float weight = kernel3[abs(xx)][abs(yy)];
      const vec2 moments = texelFetch(s_moments, p, 0).xy;
      const float variance = moments.y - moments.x * moments.x;
      sum += variance * weight;
    }
  }

  return sum;
}

layout(local_size_x = 8, local_size_y = 8) in;
void main()
{
  ivec2 gid = ivec2(gl_GlobalInvocationID.xy);
  if (any(greaterThanEqual(gid, uniforms.targetDim)))
  {
    return;
  }

  vec3 cColor = texelFetch(s_illuminance, gid, 0).rgb;
  vec3 cNormal = texelFetch(s_gBufferNormal, gid, 0).xyz;
  float cDepth = texelFetch(s_gBufferDepth, gid, 0).x;

  float variance = ComputeVarianceCenter(gid);
  float cLuminance = Luminance(cColor);
  // vec2 moments = texelFetch(s_moments, gid, 0).xy;
  // float variance = moments.y - moments.x * moments.x;

  vec2 uv = (vec2(gid) + 0.5) / uniforms.targetDim;
  vec3 point = UnprojectUV(0.1, uv, uniforms.invViewProj);
  vec3 rayDir = normalize(point - uniforms.viewPos);

  vec3 accumIlluminance = vec3(0);
  float accumWeight = 0;

  // Increase the blur width on recently disoccluded areas
  uint historyLength = 1 + texelFetch(s_historyLength, gid, 0).x;
  ivec2 stepFactor = ivec2(max(1.0, 4.0 / historyLength));

  if (uniforms.direction == ivec2(0))
  {
    for (int col = 0; col < kWidth; col++)
    {
      for (int row = 0; row < kWidth; row++)
      {
        ivec2 kernelStep = stepFactor * ivec2(uniforms.stepWidth);
        ivec2 baseOffset = ivec2(row - kRadius, col - kRadius);
        ivec2 offset = baseOffset * kernelStep;
        ivec2 id = gid + offset;
        
        if (any(greaterThanEqual(id, uniforms.targetDim)) || any(lessThan(id, ivec2(0))))
        {
          continue;
        }

        float kernelWeight = kernel[row][col];
        AddFilterContribution(accumIlluminance,
                              accumWeight,
                              cColor,
                              cLuminance,
                              cNormal,
                              cDepth,
                              variance,
                              rayDir,
                              baseOffset,
                              offset,
                              kernelStep,
                              kernelWeight,
                              id,
                              gid);
      }
    }
  }
  else
  {
    // Separable bilateral filter. Cheaper, but worse quality on edges
    for (int i = 0; i < kWidth; i++)
    {
      ivec2 kernelStep = stepFactor * ivec2(uniforms.stepWidth);
      ivec2 baseOffset = ivec2(i - kRadius) * uniforms.direction;
      ivec2 offset = baseOffset * kernelStep;
      ivec2 id = gid + offset;
      
      if (any(greaterThanEqual(id, uniforms.targetDim)) || any(lessThan(id, ivec2(0))))
      {
        continue;
      }

      float kernelWeight = kernel1D[i];
      AddFilterContribution(accumIlluminance,
                            accumWeight,
                            cColor,
                            cLuminance,
                            cNormal,
                            cDepth,
                            variance,
                            rayDir,
                            baseOffset,
                            offset,
                            kernelStep,
                            kernelWeight,
                            id,
                            gid);
    }
  }

  imageStore(i_filteredIlluminance, gid, vec4(accumIlluminance / accumWeight, 0.0));
}