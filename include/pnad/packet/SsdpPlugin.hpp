#pragma once

#include "pnad/packet/ParserInterface.hpp"

namespace asset_discovery::parser {

class SsdpPlugin final : public ParserInterface {
public:
    std::string id() const override;
    ParserMatch match(const PacketContext& context) const override;
    std::vector<AssetObservation> parse(const PacketContext& context) const override;
};

} // namespace asset_discovery::parser
