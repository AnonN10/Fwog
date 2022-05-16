#pragma once
#include <gsdf/detail/Flags.h>
#include <gsdf/BasicTypes.h>
#include <optional>
#include <span>
#include <type_traits>

namespace GFX
{
  // used to constrain types accepted by Buffer
  class TriviallyCopyableByteSpan : public std::span<const std::byte>
  {
  public:
    template<typename T> requires std::is_trivially_copyable_v<T>
    TriviallyCopyableByteSpan(const T& t) : std::span<const std::byte>(std::as_bytes(std::span{ &t, 1 })) {}

    template<typename T> requires std::is_trivially_copyable_v<T>
    TriviallyCopyableByteSpan(std::span<const T> t) : std::span<const std::byte>(std::as_bytes(t)) {}
    
    template<typename T> requires std::is_trivially_copyable_v<T>
    TriviallyCopyableByteSpan(std::span<T> t) : std::span<const std::byte>(std::as_bytes(t)) {}
  };

  enum class BufferFlag : uint32_t
  {
    NONE =            0,
    DYNAMIC_STORAGE = 1 << 0,
    CLIENT_STORAGE =  1 << 1,

    MAP_READ =        1 << 2,
    MAP_WRITE =       1 << 3,
    MAP_PERSISTENT =  1 << 4,
    MAP_COHERENT =    1 << 5,
  };
  GSDF_DECLARE_FLAG_TYPE(BufferFlags, BufferFlag, uint32_t)

  class Buffer
  {
  public:
    [[nodiscard]] static std::optional<Buffer> Create(size_t size, BufferFlags flags = BufferFlag::NONE)
    {
      return CreateInternal(nullptr, size, flags);
    }

    [[nodiscard]] static std::optional<Buffer> Create(TriviallyCopyableByteSpan data, BufferFlags flags = BufferFlag::NONE)
    {
      return CreateInternal(data.data(), data.size_bytes(), flags);
    }

    Buffer(const Buffer& other) = delete;
    Buffer(Buffer&& other) noexcept;
    Buffer& operator=(const Buffer&) = delete;
    Buffer& operator=(Buffer&& other) noexcept;
    ~Buffer();

    void SubData(TriviallyCopyableByteSpan data, size_t destOffsetBytes)
    {
      SubData(data.data(), data.size_bytes(), destOffsetBytes);
    }

    [[nodiscard]] void* GetMappedPointer();

    void UnmapPointer();

    void ClearSubData(size_t offset, size_t size, Format internalFormat, UploadFormat uploadFormat, UploadType uploadType, const void* data);

    [[nodiscard]] bool IsMapped() { return isMapped_; }

    [[nodiscard]] auto Handle() const { return id_; }

    [[nodiscard]] auto Size() const { return size_; }

  private:
    Buffer() {}
    static std::optional<Buffer> CreateInternal(const void* data, size_t size, BufferFlags flags);

    void SubData(const void* data, size_t size, size_t offset = 0);

    uint32_t id_{};
    size_t size_{};
    bool isMapped_{ false };
  };
}