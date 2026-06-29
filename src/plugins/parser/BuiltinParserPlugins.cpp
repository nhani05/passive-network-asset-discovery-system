#include "plugins/parser/BuiltinParserPlugins.hpp"

#include "plugins/parser/ArpPlugin.hpp"
#include "plugins/parser/DhcpPlugin.hpp"
#include "plugins/parser/DnsPlugin.hpp"

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
