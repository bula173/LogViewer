/**
 * @file IControler.hpp
 * @brief Interface for controller components in the MVC architecture.
 * @author LogViewer Development Team
 * @date 2025
 */
#ifndef MVC_ICONTRLOLER_HPP
#define MVC_ICONTRLOLER_HPP

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace mvc
{
/** * @class IContrloler
 * @brief Interface for controller components in the MVC architecture.
 *
 * This interface defines the methods that any controller component must
 * implement to handle user input and interact with the model and views.
 */
/**
 * @note This interface is designed to be implemented by classes that represent
 *       the controller logic in the application, coordinating between models
 * and views.
 */
struct SearchResultRow
{
    int eventId {0};
    std::string matchedText;
    std::vector<std::string> columnValues;
};

class IController
{
  public:
    /**
     * @brief Default constructor.
     *
     * Initializes the controller and prepares it for use.
     */
    IController() { }

    /**
     * @brief Default destructor.
     *
     * Cleans up resources when the controller is destroyed.
     */
    virtual ~IController() { }

    virtual std::vector<std::string> GetSearchColumns() const = 0;

    virtual void SearchEvents(const std::string& query,
      const std::vector<std::string>& columns,
      const std::function<void(const SearchResultRow&)>& onResult,
      std::function<void(size_t, size_t)> progressCallback = {}) = 0;
};

} // namespace mvc

#endif // MVC_ICONTRLOLER_HPP
