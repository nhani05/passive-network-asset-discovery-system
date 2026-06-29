#pragma once

#include "application/parser/ParserInterface.hpp"

#include <memory>
#include <vector>

namespace asset_discovery::parser {

class ParserRegistry {
public:
    void registerParser(std::unique_ptr<ParserInterface> parser);
    const std::vector<std::unique_ptr<ParserInterface>>& parsers() const;

private:
    std::vector<std::unique_ptr<ParserInterface>> parsers_;
};

} // namespace asset_discovery::parser
