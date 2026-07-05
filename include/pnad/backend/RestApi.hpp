#pragma once

#include "pnad/backend/HttpServer.hpp"

#include <string>

namespace asset_discovery::backend {

std::string jsonEscape(const std::string& value);
std::string jsonString(const std::string& value);

HttpResponse jsonSuccess(std::string dataJson, int status = 200);
HttpResponse jsonError(int status, const std::string& code, const std::string& message);

} // namespace asset_discovery::backend
