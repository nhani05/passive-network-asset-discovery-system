#pragma once

#include "application/parser/ParserInterface.hpp"

namespace asset_discovery::parser {

class ArpPlugin final : public ParserInterface {
public:
    std::string id() const override;
    ParserMatch match(const PacketContext& context) const override;
    std::vector<AssetObservation> parse(const PacketContext& context) const override;
};

} // namespace asset_discovery::parser
