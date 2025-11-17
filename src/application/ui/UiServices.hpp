#pragma once

#include "ui/IErrorPresenter.hpp"
#include "ui/NoopErrorPresenter.hpp"

#include <memory>
#include <mutex>

namespace ui {

/**
 * @brief Central registry for UI-facing service abstractions.
 */
class UiServices {
public:
    static UiServices& Instance()
    {
        static UiServices instance;
        return instance;
    }

    UiServices(const UiServices&) = delete;
    UiServices& operator=(const UiServices&) = delete;

    void SetErrorPresenter(std::shared_ptr<IErrorPresenter> presenter)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (presenter) {
            m_errorPresenter = std::move(presenter);
        } else {
            m_errorPresenter = std::make_shared<NoopErrorPresenter>();
        }
    }

    std::shared_ptr<IErrorPresenter> GetErrorPresenter() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_errorPresenter;
    }

private:
    UiServices()
        : m_errorPresenter(std::make_shared<NoopErrorPresenter>())
    {
    }

    mutable std::mutex m_mutex;
    std::shared_ptr<IErrorPresenter> m_errorPresenter;
};

} // namespace ui
