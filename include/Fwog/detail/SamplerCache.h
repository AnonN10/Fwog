#pragma once
#include "Fwog/BasicTypes.h"
#include "Fwog/Texture.h"
#include <unordered_map>

namespace std
{
  template<>
  struct hash<Fwog::SamplerState>
  {
    std::size_t operator()(const Fwog::SamplerState& k) const;
  };
} // namespace std

namespace Fwog::detail
{
  class SamplerCache
  {
  public:
    Sampler CreateOrGetCachedTextureSampler(const SamplerState& samplerState);
    size_t Size() const;
    void Clear();

  private:
    std::unordered_map<SamplerState, Sampler> samplerCache_;
  };
} // namespace Fwog::detail