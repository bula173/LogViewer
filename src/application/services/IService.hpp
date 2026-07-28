#pragma once

#include <string>
#include <memory>

namespace services {

/// Base interface for all pluggable services (AI, analyzer, exporter, etc.)
class IService {
public:
    virtual ~IService() = default;

    /// Unique identifier for this service instance
    virtual std::string GetServiceId() const = 0;

    /// Type of service ("ai", "analyzer", "exporter", etc.)
    virtual std::string GetServiceType() const = 0;
};

}  // namespace services
