#ifndef MVC_IMODEL_HPP
#define MVC_IMODEL_HPP

#include "LogEvent.hpp"
#include "IView.hpp"

#include <vector>

namespace mvc
{
/// @deprecated Prefer IModelObservable (weak_ptr-based, auto-cleanup) for new code.
class IModel
{
  public:
    IModel() = default;
    virtual ~IModel() = default;

    virtual void RegisterOndDataUpdated(IView* view)
    {
        m_views.push_back(view);
    }

    virtual void NotifyDataChanged()
    {
        for (auto* v : m_views)
            if (v) [[likely]] v->OnDataUpdated();
    }

    virtual int    GetCurrentItemIndex()       = 0;
    virtual void   SetCurrentItem(int item)    = 0;
    virtual size_t Size()                const = 0;
    virtual void   AddItem(db::LogEvent&& item) = 0;

    [[nodiscard]] virtual db::LogEvent&       GetItem(size_t index)       = 0;
    [[nodiscard]] virtual const db::LogEvent& GetItem(size_t index) const = 0;

    virtual void Clear() = 0;

  protected:
    std::vector<IView*> m_views;
};

} // namespace mvc

#endif // MVC_MODEL_HPP
