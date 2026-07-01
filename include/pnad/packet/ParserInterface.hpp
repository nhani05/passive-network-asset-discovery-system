#pragma once

#include "pnad/discovery/AssetObservation.hpp"
#include "pnad/packet/PacketContext.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace asset_discovery::parser {

struct ParserMatch {
    std::uint8_t score = 0;

    bool matched() const;
};

class ParserInterface {
public:
    virtual ~ParserInterface() = default;

    virtual std::string id() const = 0;
    virtual ParserMatch match(const PacketContext& context) const = 0;
    virtual std::vector<AssetObservation> parse(const PacketContext& context) const = 0;
};

} // namespace asset_discovery::parser
