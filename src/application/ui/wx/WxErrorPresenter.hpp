#pragma once

#include "ui/IErrorPresenter.hpp"

#include <string_view>

namespace ui::wx {

class WxErrorPresenter : public IErrorPresenter {
public:
    bool CanShowDialogs() const noexcept override;
    void ShowError(std::string_view message) override;
};

} // namespace ui::wx
