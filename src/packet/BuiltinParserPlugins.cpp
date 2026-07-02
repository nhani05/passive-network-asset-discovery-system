#include "pnad/packet/BuiltinParserPlugins.hpp"

#include "pnad/packet/ArpPlugin.hpp"
#include "pnad/packet/DhcpPlugin.hpp"
#include "pnad/packet/DnsPlugin.hpp"
#include "pnad/packet/NetbiosPlugin.hpp"
#include "pnad/packet/SsdpPlugin.hpp"
#include "pnad/packet/TcpPlugin.hpp"

#include <memory>

namespace asset_discovery::parser {

ParserRegistry createDefaultParserRegistry()
{
    ParserRegistry registry;
    registry.registerParser(std::make_unique<ArpPlugin>());
    registry.registerParser(std::make_unique<DhcpPlugin>());
    registry.registerParser(std::make_unique<DnsPlugin>());
    registry.registerParser(std::make_unique<TcpPlugin>());
    registry.registerParser(std::make_unique<SsdpPlugin>());
    registry.registerParser(std::make_unique<NetbiosPlugin>());
    return registry;
}

} // namespace asset_discovery::parser
