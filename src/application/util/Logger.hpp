/**
 * @file Logger.hpp
 * @brief Logging facade for application-wide logging
 * @author LogViewer Development Team
 * @date 2025
 *
 * Provides a clean abstraction over spdlog for better testability
 * and the ability to swap logging implementations if needed.
 */

#pragma once

#include <memory>
#include <source_location>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <string>
#include <string_view>

namespace util {

/**
 * @enum LogLevel
 * @brief Logging severity levels
 */
enum class LogLevel {
    Trace,    ///< Very detailed debug information
    Debug,    ///< Detailed debug information for development
    Info,     ///< General information about application flow
    Warning,  ///< Warning messages for potential issues
    Error,    ///< Error messages for failures
    Critical, ///< Critical errors that may cause termination
    Off       ///< Disable all logging
};

/**
 * @class ILogger
 * @brief Interface for logging implementations
 *
 * This interface allows for easy mocking in tests and provides
 * a stable API that won't change if we switch logging libraries.
 */
class ILogger {
public:
    virtual ~ILogger() = default;

    virtual void trace(std::string_view message) = 0;
    virtual void debug(std::string_view message) = 0;
    virtual void info(std::string_view message) = 0;
    virtual void warn(std::string_view message) = 0;
    virtual void error(std::string_view message) = 0;
    virtual void critical(std::string_view message) = 0;

    virtual void setLevel(LogLevel level) = 0;
    virtual LogLevel getLevel() const = 0;
};

/**
 * @class SpdLogger
 * @brief spdlog-based implementation of ILogger
 *
 * Wraps spdlog with our ILogger interface for dependency injection
 * and testing purposes.
 */
class SpdLogger : public ILogger {
public:
    /**
     * @brief Constructs a logger with given name
     * @param name Logger name (used in log output)
     */
    explicit SpdLogger(std::string name = "LogViewer")
        : m_logger(spdlog::get(name))
    {
        if (!m_logger) {
            m_logger = spdlog::default_logger();
        }
    }

    void trace(std::string_view message) override
    {
        m_logger->trace(message);
    }

    void debug(std::string_view message) override
    {
        m_logger->debug(message);
    }

    void info(std::string_view message) override { m_logger->info(message); }

    void warn(std::string_view message) override { m_logger->warn(message); }

    void error(std::string_view message) override
    {
        m_logger->error(message);
    }

    void critical(std::string_view message) override
    {
        m_logger->critical(message);
    }

    void setLevel(LogLevel level) override
    {
        m_logger->set_level(toSpdlogLevel(level));
    }

    LogLevel getLevel() const override
    {
        return fromSpdlogLevel(m_logger->level());
    }

private:
    static spdlog::level::level_enum toSpdlogLevel(LogLevel level)
    {
        switch (level) {
        case LogLevel::Trace:
            return spdlog::level::trace;
        case LogLevel::Debug:
            return spdlog::level::debug;
        case LogLevel::Info:
            return spdlog::level::info;
        case LogLevel::Warning:
            return spdlog::level::warn;
        case LogLevel::Error:
            return spdlog::level::err;
        case LogLevel::Critical:
            return spdlog::level::critical;
        case LogLevel::Off:
            return spdlog::level::off;
        }
        return spdlog::level::info;
    }

    static LogLevel fromSpdlogLevel(spdlog::level::level_enum level)
    {
        switch (level) {
        case spdlog::level::trace:
            return LogLevel::Trace;
        case spdlog::level::debug:
            return LogLevel::Debug;
        case spdlog::level::info:
            return LogLevel::Info;
        case spdlog::level::warn:
            return LogLevel::Warning;
        case spdlog::level::err:
            return LogLevel::Error;
        case spdlog::level::critical:
            return LogLevel::Critical;
        case spdlog::level::off:
            return LogLevel::Off;
        default:
            return LogLevel::Info;
        }
    }

    std::shared_ptr<spdlog::logger> m_logger;
};

/**
 * @class Logger
 * @brief Application-wide logging singleton
 *
 * Provides convenient static methods for logging throughout the application.
 * Can be configured with a custom ILogger implementation for testing.
 *
 * @par Example Usage:
 * @code
 * Logger::Info("Application started");
 * Logger::Debug("Processing file: {}", filename);
 * Logger::Error("Failed to load config: {}", error);
 * @endcode
 *
 * @par Testing:
 * @code
 * // In tests, inject a mock logger
 * auto mockLogger = std::make_shared<MockLogger>();
 * Logger::SetInstance(mockLogger);
 * @endcode
 */
class Logger {
public:
    /**
     * @brief Initializes the logger with default configuration
     * @param level Initial logging level
     * @param logFile Optional file path for file logging
     */
    static void Initialize(LogLevel level = LogLevel::Info,
        const std::string& logFile = "")
    {
        if (s_instance) {
            return; // Already initialized
        }

        s_instance = std::make_shared<SpdLogger>();
        s_instance->setLevel(level);

        // Additional spdlog configuration can go here
        if (!logFile.empty()) {
            spdlog::default_logger()->sinks().push_back(
                std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFile));
        }
    }

    /**
     * @brief Sets a custom logger instance (for testing)
     * @param logger Custom logger implementation
     */
    static void SetInstance(std::shared_ptr<ILogger> logger)
    {
        s_instance = std::move(logger);
    }

    /**
     * @brief Gets the current logger instance
     * @return Shared pointer to logger
     */
    static std::shared_ptr<ILogger> GetInstance()
    {
        if (!s_instance) {
            Initialize();
        }
        return s_instance;
    }

    // Convenience methods

    template<typename... Args>
    static void Trace(fmt::format_string<Args...> fmt, Args&&... args)
    {
        GetInstance()->trace(fmt::format(fmt, std::forward<Args>(args)...));
    }

    template<typename... Args>
    static void Debug(fmt::format_string<Args...> fmt, Args&&... args)
    {
        GetInstance()->debug(fmt::format(fmt, std::forward<Args>(args)...));
    }

    template<typename... Args>
    static void Info(fmt::format_string<Args...> fmt, Args&&... args)
    {
        GetInstance()->info(fmt::format(fmt, std::forward<Args>(args)...));
    }

    template<typename... Args>
    static void Warn(fmt::format_string<Args...> fmt, Args&&... args)
    {
        GetInstance()->warn(fmt::format(fmt, std::forward<Args>(args)...));
    }

    template<typename... Args>
    static void Error(fmt::format_string<Args...> fmt, Args&&... args)
    {
        GetInstance()->error(fmt::format(fmt, std::forward<Args>(args)...));
    }

    template<typename... Args>
    static void Critical(fmt::format_string<Args...> fmt, Args&&... args)
    {
        GetInstance()->critical(fmt::format(fmt, std::forward<Args>(args)...));
    }

    /**
     * @brief Sets the global logging level
     * @param level New logging level
     */
    static void SetLevel(LogLevel level)
    {
        GetInstance()->setLevel(level);
    }

    /**
     * @brief Gets the current logging level
     * @return Current logging level
     */
    static LogLevel GetLevel() { return GetInstance()->getLevel(); }

private:
    static std::shared_ptr<ILogger> s_instance;
};

// Definition must be in .cpp file
inline std::shared_ptr<ILogger> Logger::s_instance = nullptr;

} // namespace util
