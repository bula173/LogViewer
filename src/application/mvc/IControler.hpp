/**
 * @file IControler.hpp
 * @brief Interface for controller components in the MVC architecture.
 * @author LogViewer Development Team
 * @date 2025
 *
 * @note The file name retains its original spelling for backward compatibility
 *       with existing include directives.  The class inside is @c IController
 *       (correct spelling).
 */
#ifndef MVC_ICONTRLOLER_HPP
#define MVC_ICONTRLOLER_HPP

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace parser
{
class IDataParserObserver;
}

namespace mvc
{
/**
 * @brief Result row returned by IController::SearchEvents() for each matching event.
 *
 * Contains the event ID, the field that matched, the matched text, and a
 * column-value snapshot that the search-results panel can display directly
 * without re-querying the container.
 */
struct SearchResultRow
{
    int                      eventId {0};
    std::string              matchedKey;
    std::string              matchedText;
    std::vector<std::string> columnValues;
};

/**
 * @class IController
 * @brief Toolkit-agnostic controller interface for the MVC architecture.
 *
 * Implementations coordinate between the data model (EventsContainer) and
 * the view layer.  The default implementation is @c mvc::MainController.
 *
 * @note The file that declares this interface is named @c IControler.hpp
 *       (one 'l') for historical reasons.  Include that file, not this
 *       Doxygen-rendered name.
 */
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

    /** @brief Parses and loads the provided log file into the model. */
    virtual void LoadLogFile(const std::filesystem::path& filepath,
        parser::IDataParserObserver* observer) = 0;

    /** @brief @return current parser progress (0 if idle). */
    virtual uint32_t GetParserCurrentProgress() const = 0;

    /** @brief @return parser total progress target (0 if unknown/idle). */
    virtual uint32_t GetParserTotalProgress() const = 0;
};

} // namespace mvc

#endif // MVC_ICONTRLOLER_HPP
