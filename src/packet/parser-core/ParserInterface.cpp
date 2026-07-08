#include "pnad/packet/ParserInterface.hpp"

namespace asset_discovery::parser {

bool ParserMatch::matched() const
{
    return score > 0;
}

} // namespace asset_discovery::parser
