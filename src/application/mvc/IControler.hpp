/**
 * @file IControler.hpp
 * @brief Interface for controller components in the MVC architecture.
 * @author LogViewer Development Team
 * @date 2025
 */
#ifndef MVC_ICONTRLOLER_HPP
#define MVC_ICONTRLOLER_HPP

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
};

} // namespace mvc

#endif // MVC_ICONTRLOLER_HPP
