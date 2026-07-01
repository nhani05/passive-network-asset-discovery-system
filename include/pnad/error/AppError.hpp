#pragma once

#include <stdexcept>

namespace asset_discovery {

class AppError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class ConfigError : public AppError {
public:
    using AppError::AppError;
};

class CaptureError : public AppError {
public:
    using AppError::AppError;
};

class PcapError : public AppError {
public:
    using AppError::AppError;
};

class DatabaseError : public AppError {
public:
    using AppError::AppError;
};

} // namespace asset_discovery
