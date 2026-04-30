#pragma once

#include "EventsContainer.hpp"
#include "IControler.hpp"

#include "IDataParser.hpp"

#include <filesystem>
#include <memory>

namespace mvc {

class MainController : public IController, public parser::IDataParserObserver {
public:
    explicit MainController(db::EventsContainer& events);

    [[nodiscard]] std::vector<std::string> GetSearchColumns() const override;

    void SearchEvents(const std::string& query,
        const std::vector<std::string>& columns,
        const std::function<void(const SearchResultRow&)>& onResult,
        std::function<void(size_t, size_t)> progressCallback = {}) override;

    void LoadLogFile(const std::filesystem::path& filepath,
        parser::IDataParserObserver* observer) override;

    uint32_t GetParserCurrentProgress() const override;
    uint32_t GetParserTotalProgress() const override;

    // parser::IDataParserObserver
    void ProgressUpdated() override;
    void NewEventFound(db::LogEvent&& event) override;
    void NewEventBatchFound(std::vector<std::pair<int,
        db::LogEvent::EventItems>>&& eventBatch) override;

private:
    db::EventsContainer& m_events;
    std::unique_ptr<parser::IDataParser> m_activeParser;
};

} // namespace mvc
