#pragma once

#include "pnad/packet/ParserRegistry.hpp"

namespace asset_discovery::parser {

ParserRegistry createDefaultParserRegistry();
ParserRegistry createCoreParserRegistry();

} // namespace asset_discovery::parser
