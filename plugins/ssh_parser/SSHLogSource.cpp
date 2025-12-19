/**
 * @file SSHLogSource.cpp
 * @brief Implementation of SSH log source
 * @author LogViewer Development Team
 * @date 2025
 */

#include "SSHLogSource.hpp"
#include "Logger.hpp"
#include <chrono>
#include <sstream>
#include <thread>

namespace ssh
{

SSHLogSource::SSHLogSource(const SSHLogSourceConfig& config)
    : m_config(config)
    , m_connection(std::make_unique<SSHConnection>(config.connectionConfig))
    , m_parser(std::make_unique<SSHTextParser>())
{
    util::Logger::Debug("SSHLogSource constructor called");
}

SSHLogSource::~SSHLogSource()
{
    Stop();
}

SSHLogSource::SSHLogSource(SSHLogSource&& other) noexcept
    : m_config(std::move(other.m_config))
    , m_connection(std::move(other.m_connection))
    , m_parser(std::move(other.m_parser))
    , m_monitorThread(std::move(other.m_monitorThread))
    , m_running(other.m_running.load())
    , m_lastError(std::move(other.m_lastError))
    , m_lineBuffer(std::move(other.m_lineBuffer))
{
    other.m_running = false;
}

SSHLogSource& SSHLogSource::operator=(SSHLogSource&& other) noexcept
{
    if (this != &other)
    {
        Stop();

        m_config = std::move(other.m_config);
        m_connection = std::move(other.m_connection);
        m_parser = std::move(other.m_parser);
        m_monitorThread = std::move(other.m_monitorThread);
        m_running = other.m_running.load();
        m_lastError = std::move(other.m_lastError);
        m_lineBuffer = std::move(other.m_lineBuffer);

        other.m_running = false;
    }
    return *this;
}

bool SSHLogSource::Start()
{
    if (m_running)
    {
        m_lastError = "Already running";
        return false;
    }

    util::Logger::Info("Starting SSH log source");

    // Connect to SSH host
    if (!m_connection->Connect())
    {
        m_lastError = "Failed to connect: " + m_connection->GetLastError();
        util::Logger::Error("{}", m_lastError);
        return false;
    }

    // Start monitoring thread
    m_running = true;
    m_monitorThread = std::make_unique<std::thread>(&SSHLogSource::MonitoringThread, this);

    util::Logger::Info("SSH log source started successfully");
    return true;
}

void SSHLogSource::Stop()
{
    if (!m_running)
    {
        return;
    }

    util::Logger::Info("Stopping SSH log source");
    m_running = false;

    if (m_monitorThread && m_monitorThread->joinable())
    {
        m_monitorThread->join();
    }

    m_connection->Disconnect();
    util::Logger::Info("SSH log source stopped");
}

bool SSHLogSource::IsRunning() const
{
    return m_running;
}

SSHTextParser& SSHLogSource::GetParser()
{
    return *m_parser;
}

SSHConnection& SSHLogSource::GetConnection()
{
    return *m_connection;
}

void SSHLogSource::RegisterObserver(parser::IDataParserObserver* observer)
{
    m_parser->RegisterObserver(observer);
}

void SSHLogSource::UnregisterObserver(parser::IDataParserObserver* observer)
{
    m_parser->UnregisterObserver(observer);
}

std::string SSHLogSource::GetLastError() const
{
    return m_lastError;
}

void SSHLogSource::MonitoringThread()
{
    util::Logger::Debug("SSH monitoring thread started");

    try
    {
        switch (m_config.mode)
        {
            case SSHLogSourceMode::CommandOutput:
                MonitorCommandOutput();
                break;

            case SSHLogSourceMode::TailFile:
                MonitorTailFile();
                break;

            case SSHLogSourceMode::InteractiveShell:
                MonitorInteractiveShell();
                break;
        }
    }
    catch (const std::exception& e)
    {
        m_lastError = std::string("Monitoring error: ") + e.what();
        util::Logger::Error("{}", m_lastError);
    }

    m_running = false;
    util::Logger::Debug("SSH monitoring thread exited");
}

void SSHLogSource::MonitorCommandOutput()
{
    util::Logger::Info("Monitoring command output: {}", m_config.command);

    std::string output;
    int exitCode = m_connection->ExecuteCommand(m_config.command, output);

    if (exitCode == 0)
    {
        // Parse the output
        std::istringstream stream(output);
        m_parser->ParseData(stream);
    }
    else
    {
        m_lastError = "Command failed with exit code: " + std::to_string(exitCode);
        util::Logger::Error("{}", m_lastError);
    }
}

void SSHLogSource::MonitorTailFile()
{
    util::Logger::Info("Monitoring log file: {}", m_config.logFilePath);

    // Construct tail command with follow option
    std::string command = "tail -f " + m_config.logFilePath;

    if (!m_connection->OpenShell())
    {
        m_lastError = "Failed to open shell: " + m_connection->GetLastError();
        util::Logger::Error("{}", m_lastError);
        return;
    }

    // Send tail command
    m_connection->WriteShellInput(command + "\n");

    char buffer[4096];
    int eventId = 0;

    while (m_running)
    {
        int nbytes = m_connection->ReadShellOutput(buffer, sizeof(buffer) - 1);

        if (nbytes > 0)
        {
            buffer[nbytes] = '\0';
            ProcessData(buffer, nbytes);
        }
        else if (nbytes < 0)
        {
            if (m_config.autoReconnect)
            {
                util::Logger::Warn("Connection lost, attempting reconnect...");
                m_connection->Disconnect();
                
                if (m_connection->Connect())
                {
                    if (!m_connection->OpenShell())
                    {
                        break;
                    }
                    m_connection->WriteShellInput(command + "\n");
                }
                else
                {
                    break;
                }
            }
            else
            {
                break;
            }
        }
        else
        {
            // No data available, sleep briefly
            std::this_thread::sleep_for(
                std::chrono::milliseconds(m_config.pollIntervalMs));
        }
    }

    m_connection->CloseShell();
}

void SSHLogSource::MonitorInteractiveShell()
{
    util::Logger::Info("Monitoring interactive shell");

    if (!m_connection->OpenShell())
    {
        m_lastError = "Failed to open shell: " + m_connection->GetLastError();
        util::Logger::Error("{}", m_lastError);
        return;
    }

    char buffer[4096];

    while (m_running)
    {
        int nbytes = m_connection->ReadShellOutput(buffer, sizeof(buffer) - 1);

        if (nbytes > 0)
        {
            buffer[nbytes] = '\0';
            ProcessData(buffer, nbytes);
        }
        else if (nbytes < 0)
        {
            break;
        }
        else
        {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(m_config.pollIntervalMs));
        }
    }

    m_connection->CloseShell();
}

void SSHLogSource::ProcessData(const char* buffer, size_t size)
{
    // Accumulate data into line buffer
    m_lineBuffer.append(buffer, size);

    // Process complete lines
    size_t pos;
    static int eventId = 0;
    std::vector<std::pair<int, db::LogEvent::EventItems>> eventBatch;

    while ((pos = m_lineBuffer.find('\n')) != std::string::npos)
    {
        std::string line = m_lineBuffer.substr(0, pos);
        m_lineBuffer.erase(0, pos + 1);

        // Remove carriage return if present
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }

        if (!line.empty())
        {
            try
            {
                db::LogEvent::EventItems items = m_parser->ParseLine(line, ++eventId);
                if (!items.empty())
                {
                    eventBatch.emplace_back(eventId, std::move(items));

                    // Send batch periodically
                    if (eventBatch.size() >= 10)
                    {
                        m_parser->NotifyNewEventBatch(std::move(eventBatch));
                        eventBatch.clear();
                    }
                }
            }
            catch (const std::exception& e)
            {
                util::Logger::Warn("Failed to parse line: {}", e.what());
            }
        }
    }

    // Send any remaining events
    if (!eventBatch.empty())
    {
        m_parser->NotifyNewEventBatch(std::move(eventBatch));
    }
}

} // namespace ssh
