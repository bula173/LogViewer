#pragma once

#include "db/EventsContainer.hpp"
#include "mvc/IControler.hpp"

namespace mvc {

class MainController : public IController {
public:
    explicit MainController(db::EventsContainer& events);

    std::vector<std::string> GetSearchColumns() const override;

    void SearchEvents(const std::string& query,
        const std::vector<std::string>& columns,
        const std::function<void(const SearchResultRow&)>& onResult,
        std::function<void(size_t, size_t)> progressCallback = {}) override;

private:
    db::EventsContainer& m_events;
};

} // namespace mvc
