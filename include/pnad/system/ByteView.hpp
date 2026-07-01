#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace asset_discovery {

struct ByteView {
    const std::uint8_t* data = nullptr;
    std::size_t size = 0;

    bool empty() const
    {
        return size == 0;
    }

    const std::uint8_t& operator[](std::size_t index) const
    {
        return data[index];
    }

    const std::uint8_t* begin() const
    {
        return data;
    }

    const std::uint8_t* end() const
    {
        return size == 0 ? data : data + size;
    }

    ByteView subview(std::size_t offset) const
    {
        if (offset >= size) {
            return {};
        }
        return {data + offset, size - offset};
    }

    ByteView subview(std::size_t offset, std::size_t length) const
    {
        if (offset >= size) {
            return {};
        }
        const auto available = size - offset;
        return {data + offset, std::min(length, available)};
    }
};

inline ByteView makeByteView(const std::vector<std::uint8_t>& bytes)
{
    return {bytes.empty() ? nullptr : bytes.data(), bytes.size()};
}

inline std::vector<std::uint8_t> copyBytes(ByteView view)
{
    return std::vector<std::uint8_t>(view.begin(), view.end());
}

inline bool operator==(ByteView left, const std::vector<std::uint8_t>& right)
{
    if (left.size != right.size()) {
        return false;
    }
    return std::equal(left.begin(), left.end(), right.begin());
}

inline bool operator==(const std::vector<std::uint8_t>& left, ByteView right)
{
    return right == left;
}

inline bool operator!=(ByteView left, const std::vector<std::uint8_t>& right)
{
    return !(left == right);
}

inline bool operator!=(const std::vector<std::uint8_t>& left, ByteView right)
{
    return !(right == left);
}

} // namespace asset_discovery
