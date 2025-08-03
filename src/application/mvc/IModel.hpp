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
/**
 * @class IModel
 * @brief Interface for model components in the MVC architecture.
 *
 * This interface defines the methods that any model component must implement
 * to manage data and notify views about changes.
 */
class IModel
{
  public:
    /**
     * @brief Default constructor.
     *
     * Initializes the model and prepares it for use.
     */
    IModel()
    {
        spdlog::debug("IModel::IModel constructed");
    }
    /**
     * @brief Default destructor.
     *
     * Cleans up resources and unregisters views when the model is destroyed.
     */
    /**
     * @note This destructor is virtual to ensure proper cleanup of derived
     * classes.
     */
    virtual ~IModel()
    {
        spdlog::debug("IModel::~IModel destructed, clearing views");
        m_views.clear();
    }
    /**
     * @brief Registers a view to receive data update notifications.
     *
     * @param view Pointer to the view object to register
     * @note The view must implement the IView interface and remain valid for
     * the lifetime of the model
     */
    virtual void RegisterOndDataUpdated(IView* view)
    {
        spdlog::debug("IModel::RegisterOndDataUpdated called");
        m_views.push_back(view);
    }

    /**
     * @brief Notifies all registered views that the model's data has changed.
     *
     * This method should be called whenever the model's data is updated to
     * ensure all views are refreshed with the latest information.
     *
     * @note Views should implement OnDataUpdated() to handle this notification
     */
    virtual void NotifyDataChanged()
    {
        spdlog::debug("IModel::NotifyDataChanged called");
        for (auto v : m_views)
        {
            v->OnDataUpdated();
        }
    }

    /**
     * @brief Notifies all registered views that the current item index has
     * changed.
     *
     * @param index The new current item index in the model
     * @note Views should implement OnCurrentIndexUpdated() to handle this
     * notification
     */
    virtual int GetCurrentItemIndex() = 0;
    /**
     * @brief Sets the current item index in the model.
     *
     * @param item The new current item index
     */
    virtual void SetCurrentItem(const int item) = 0;
    /**
     * @brief Gets the total number of items in the model.
     *
     * @return size_t The total number of items
     */
    virtual size_t Size() = 0;
    /**
     * @brief Adds a new item to the model.
     *
     * @param item The item to add (moved to avoid copying)
     * @note This method should be called when a new item is created or parsed
     */
    virtual void AddItem(db::LogEvent&& item) = 0;
    /**
     * @brief Gets an item from the model.
     *
     * @param index The index of the item to retrieve
     * @return db::LogEvent& A reference to the requested item
     */
    virtual db::LogEvent& GetItem(const int index) = 0;
    /**
     * @brief Clears all items from the model.
     *
     * This method should be called when the model needs to be reset or emptied.
     * It will also notify all views that the data has been cleared.
     */
    virtual void Clear() = 0;

  protected:
    std::vector<IView*>
        m_views; ///< List of registered views to notify on data changes
};

} // namespace mvc

#endif // MVC_MODEL_HPP
