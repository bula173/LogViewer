#ifndef MVC_IMODEL_HPP
#define MVC_IMODEL_HPP

#include "db/LogEvent.hpp"
#include "mvc/IView.hpp"

#include <iostream>
#include <mutex>
#include <spdlog/spdlog.h>
#include <vector>

namespace mvc
{
class IModel
{
  public:
    IModel()
    {
        spdlog::debug("IModel::IModel constructed");
    }
    virtual ~IModel()
    {
        spdlog::debug("IModel::~IModel destructed, clearing views");
        m_views.clear();
    }
    virtual void RegisterOndDataUpdated(IView* view)
    {
        spdlog::debug("IModel::RegisterOndDataUpdated called");
        m_views.push_back(view);
    }

    virtual void NotifyDataChanged()
    {
        spdlog::debug("IModel::NotifyDataChanged called");
        for (auto v : m_views)
        {
            v->OnDataUpdated();
        }
    }

    virtual int GetCurrentItemIndex() = 0;
    virtual void SetCurrentItem(const int item) = 0;
    virtual void AddItem(db::LogEvent&& item) = 0;
    virtual db::LogEvent& GetItem(const int index) = 0;
    virtual void Clear() = 0;
    virtual size_t Size() = 0;

  protected:
    std::vector<IView*> m_views;
};

} // namespace mvc

#endif // MVC_MODEL_HPP
