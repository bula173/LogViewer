#pragma once

#include "ui/IErrorPresenter.hpp"
#include "util/Logger.hpp"

#include <mutex>
#include <string_view>

namespace ui {

/**
 * @brief Logger-backed fallback when no concrete UI presenter is registered.
 */
class NoopErrorPresenter : public IErrorPresenter {
public:
    bool CanShowDialogs() const noexcept override { return false; }

    void ShowError(std::string_view message) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_loggedOnce) {
            util::Logger::Warn(
                "UI error presenter not configured; falling back to logs");
            m_loggedOnce = true;
        }
        util::Logger::Error("{}", message);
    }

private:
    mutable std::mutex m_mutex;
    bool m_loggedOnce = false;
};

} // namespace ui
