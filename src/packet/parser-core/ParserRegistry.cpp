#include "pnad/packet/ParserRegistry.hpp"

#include <utility>

namespace asset_discovery::parser {

void ParserRegistry::registerParser(std::unique_ptr<ParserInterface> parser)
{
    if (parser) {
        parsers_.push_back(std::move(parser));
    }
}

const std::vector<std::unique_ptr<ParserInterface>>& ParserRegistry::parsers() const
{
    return parsers_;
}

} // namespace asset_discovery::parser
