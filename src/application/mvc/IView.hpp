/**
 * @file IView.hpp
 * @brief Interface for view components in the MVC architecture.
 * @author LogViewer Development Team
 * @date 2025
 */


#pragma once

namespace mvc
{
/**
 * @class IView
 * @brief Interface for view components in the MVC architecture.
 *
 * This interface defines the methods that any view component must implement
 * to receive updates from the model. It allows the model to notify views about
 * data changes and current index updates.
 */
/**
 * @note This interface is designed to be implemented by classes that represent
 *       the user interface components displaying data from the model.
 */
class IView
{
  public:
    /**
     * @brief Default constructor.
     */
    IView() { }
    /**
     * @brief Default destructor.
     *
     * Ensures proper cleanup of resources when the view is destroyed.
     */
    virtual ~IView() { }
    /**
     * @brief Called when the model's data is updated.
     *
     * This method should be implemented to refresh the view with the latest
     * data from the model.
     */
    virtual void OnDataUpdated() = 0;
    /**
     * @brief Called when the current item index in the model is updated.
     *
     * This method should be implemented to update the view based on the new
     * current item index.
     *
     * @param index The new current item index in the model
     */
    virtual void OnCurrentIndexUpdated(const int index) = 0;
};

} // namespace mvc
