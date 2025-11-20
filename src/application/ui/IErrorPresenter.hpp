#pragma once

#include <string_view>

namespace ui {

/**
 * @brief Presents user-facing error information via the current UI backend.
 */
class IErrorPresenter {
public:
    virtual ~IErrorPresenter() = default;

    /**
     * @return true if the underlying UI backend can currently display dialogs.
     */
    virtual bool CanShowDialogs() const noexcept = 0;

    /**
     * @brief Display an error message to the user.
     */
    virtual void ShowError(std::string_view message) = 0;
};

} // namespace ui
