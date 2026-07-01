#include "pnad/packet/BuiltinParserPlugins.hpp"

#include "pnad/packet/ArpPlugin.hpp"
#include "pnad/packet/DhcpPlugin.hpp"
#include "pnad/packet/DnsPlugin.hpp"

#include <memory>

namespace asset_discovery::parser {

ParserRegistry createDefaultParserRegistry()
{
    ParserRegistry registry;
    registry.registerParser(std::make_unique<ArpPlugin>());
    registry.registerParser(std::make_unique<DhcpPlugin>());
    registry.registerParser(std::make_unique<DnsPlugin>());
    return registry;
}

} // namespace asset_discovery::parser
