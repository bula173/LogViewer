/**
 * @file IModelObservable.hpp
 * @brief Modern C++20 observer pattern with automatic lifetime management
 * @author LogViewer Development Team
 * @date 2026
 */

#ifndef MVC_MODEL_OBSERVABLE_HPP
#define MVC_MODEL_OBSERVABLE_HPP

#include "IView.hpp"
#include "Logger.hpp"
#include <algorithm>
#include <memory>
#include <shared_mutex>
#include <vector>

namespace mvc {

/**
 * @class IModelObservable
 * @brief Improved IModel with weak_ptr observers for automatic cleanup
 *
 * This interface replaces the raw pointer-based observer pattern with
 * weak_ptr-based observers that automatically detect and remove destroyed views.
 * It provides thread-safe observer notifications with reader-writer locking.
 *
 * @par Thread Safety
 * All observer management and notification methods are thread-safe using
 * shared_mutex for concurrent reads and exclusive writes.
 *
 * @par Lifetime Management
 * - Views register as shared_ptr, internally stored as weak_ptr
 * - Destroyed views are automatically detected and cleaned up
 * - No dangling pointers possible
 * - Views control their own lifetime via shared_ptr reference counting
 *
 * @par Migration Guide
 * Old API (deprecated):
 * @code
 * model->RegisterOndDataUpdated(view.get());  // Raw pointer
 * @endcode
 *
 * New API (recommended):
 * @code
 * auto view = std::make_shared<MyView>();
 * model->RegisterView(view);  // Shared pointer - safe!
 * @endcode
 *
 * @see IModel for backward compatibility interface
 */
class IModelObservable {
  public:
    /// @brief Shared pointer type for views
    using ViewPtr = std::shared_ptr<IView>;

    virtual ~IModelObservable() = default;

    /**
     * @brief Registers a view as observer with automatic cleanup
     *
     * The view is stored internally as weak_ptr so it's automatically
     * removed when destroyed. No dangling pointers.
     *
     * @param view Shared pointer to view to register
     *
     * @par Example
     * @code
     * auto container = std::make_shared<EventsContainer>();
     * auto view = std::make_shared<ItemDetailsView>();
     * container->RegisterView(view);  // Safe lifetime management
     * view.reset();  // View destroyed
     * container->NotifyDataChanged();  // Safe: weak_ptr detects deletion
     * @endcode
     */
    void RegisterView(ViewPtr view)
    {
        if (!view) {
            util::Logger::Warn("Attempt to register null view");
            return;
        }

        {
            std::unique_lock lock(m_viewsMutex);
            m_weakViews.push_back(std::weak_ptr<IView>(view));
            CleanupDeadViews();
            util::Logger::Trace("View registered, total: {} active views",
                               m_weakViews.size());
        }
        // Call into view AFTER releasing the lock to avoid re-entrancy deadlock
        view->OnAttachedToModel(this);
    }

    /**
     * @brief Unregisters a view from observations
     *
     * @param view Pointer to the view to unregister
     *
     * Safe if view already unregistered or destroyed.
     */
    void UnregisterView(const IView* view)
    {
        if (!view) {
            return;
        }

        ViewPtr found;
        {
            std::unique_lock lock(m_viewsMutex);

            for (auto& weak : m_weakViews) {
                if (auto v = weak.lock(); v && v.get() == view) {
                    found = v;
                    break;
                }
            }

            m_weakViews.erase(
                std::remove_if(m_weakViews.begin(), m_weakViews.end(),
                               [view](const auto& weak) {
                                   auto shared = weak.lock();
                                   return !shared || shared.get() == view;
                               }),
                m_weakViews.end());

            util::Logger::Trace("View unregistered, remaining: {} views",
                               m_weakViews.size());
        }
        // Call into view AFTER releasing the lock
        if (found) {
            found->OnDetachedFromModel();
        }
    }

    /**
     * @brief Gets count of currently active observers
     * @return Number of non-expired weak_ptr observers
     */
    size_t GetViewCount() const
    {
        std::unique_lock lock(m_viewsMutex);
        // Prune expired entries so the count reflects live observers only
        m_weakViews.erase(
            std::remove_if(m_weakViews.begin(), m_weakViews.end(),
                           [](const auto& w) { return w.expired(); }),
            m_weakViews.end());
        return m_weakViews.size();
    }

  protected:
    /**
     * @brief Notifies all registered views that data has changed
     *
     * Iterates through all observers and calls their OnDataUpdated method.
     * Dead observers (destroyed views) are automatically detected via weak_ptr
     * and skipped without error.
     *
     * @par Exception Safety
     * If a view's OnDataUpdated throws, the exception propagates after
     * notifying remaining views. Uses best-effort notification.
     */
    void NotifyDataChanged()
    {
        // Snapshot live views under lock, then notify outside the lock.
        // This prevents deadlock from lock upgrade (shared→unique) and
        // re-entrancy if a view calls back into the model during notification.
        std::vector<ViewPtr> live;
        {
            std::unique_lock lock(m_viewsMutex);
            CleanupDeadViews();
            live.reserve(m_weakViews.size());
            for (auto& weak : m_weakViews) {
                if (auto v = weak.lock()) live.push_back(v);
            }
        }

        for (auto& view : live) {
            try {
                view->OnDataUpdated();
            } catch (const std::exception& e) {
                util::Logger::Error("Observer error in OnDataUpdated: {}",
                                   e.what());
            }
        }
    }

    /**
     * @brief Notifies all views of current item index change
     *
     * @param index The new current item index
     */
    void NotifyCurrentIndexUpdated(int index)
    {
        std::vector<ViewPtr> live;
        {
            std::unique_lock lock(m_viewsMutex);
            live.reserve(m_weakViews.size());
            for (auto& weak : m_weakViews) {
                if (auto v = weak.lock()) live.push_back(v);
            }
        }

        for (auto& view : live) {
            try {
                view->OnCurrentIndexUpdated(index);
            } catch (const std::exception& e) {
                util::Logger::Error("Observer error in OnCurrentIndexUpdated: {}",
                                   e.what());
            }
        }
    }

    /**
     * @brief Manually triggers cleanup of dead observers
     *
     * Can be called explicitly to prune expired weak_ptr entries.
     * Internally, cleanup is also performed on every RegisterView and
     * NotifyDataChanged call.
     */
    void CleanupDeadObservers()
    {
        std::unique_lock lock(m_viewsMutex);
        CleanupDeadViews();
    }

  private:
    /// Storage for observer weak pointers - automatically manages cleanup
    mutable std::shared_mutex m_viewsMutex;
    std::vector<std::weak_ptr<IView>> m_weakViews;

    /**
     * @brief Internal method to clean up expired weak pointers
     *
     * Called during registration and notifications to remove dead observers.
     * Should only be called with lock held.
     */
    void CleanupDeadViews() const
    {
        // Note: Intentionally not calling const_cast - relies on thread safety
        // The mutable m_viewsMutex allows shared_lock const access with modification
        auto& mutable_views = const_cast<std::vector<std::weak_ptr<IView>>&>(m_weakViews);

        auto erased = std::remove_if(
            mutable_views.begin(), mutable_views.end(),
            [](const auto& weak) { return weak.expired(); });

        size_t count = std::distance(erased, mutable_views.end());
        if (count > 0) {
            util::Logger::Trace("Cleaned up {} expired view observers", count);
            mutable_views.erase(erased, mutable_views.end());
        }
    }
};

} // namespace mvc

#endif // MVC_MODEL_OBSERVABLE_HPP
