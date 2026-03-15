/**
 * @file PluginFunctionRegistry.hpp
 * @brief Type-safe registry for dynamically loaded plugin functions
 * @author LogViewer Development Team
 * @date 2026
 */

#ifndef PLUGIN_FUNCTION_REGISTRY_HPP
#define PLUGIN_FUNCTION_REGISTRY_HPP

#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include "Logger.hpp"

#ifdef _WIN32
    #include <windows.h>
#else
    #include <dlfcn.h>
#endif

namespace plugin {

/**
 * @class FunctionRegistry
 * @brief Type-safe wrapper for dynamically loaded functions
 *
 * Provides compile-time type safety for C-style function pointers
 * loaded from dynamic libraries via dlsym/GetProcAddress.
 *
 * @tparam FnType Function pointer type (e.g., void(*)(int, const char*))
 *
 * @par Usage Example
 * @code
 * using CreatePanelFn = void*(*)(void*, void*, const char*);
 * FunctionRegistry<CreatePanelFn> creator(dllHandle, "Plugin_CreateLeftPanel");
 *
 * if (creator) {  // operator bool checks if symbol was found
 *     auto* widget = creator.call(handle, parent, settings);
 * }
 * @endcode
 *
 * @par Benefits
 * - Compile-time function signature verification
 * - No manual reinterpret_cast needed
 * - Clear error handling
 * - Type-safe invocation with concepts
 */
template <typename FnType>
class FunctionRegistry {
  public:
    /**
     * @brief Constructs registry and attempts to resolve symbol
     * @param handle Dynamic library handle from dlopen/LoadLibrary
     * @param symbolName C-string name of the symbol to resolve
     */
    FunctionRegistry(void* handle, const std::string& symbolName)
        : m_handle(handle), m_symbolName(symbolName), m_function(nullptr)
    {
        m_function = ResolveSymbol();

        if (!m_function) {
            util::Logger::Debug("Plugin symbol not found: {}", symbolName);
        } else {
            util::Logger::Trace("Plugin symbol resolved: {}", symbolName);
        }
    }

    /**
     * @brief Checks if symbol was successfully resolved
     * @return true if function symbol is available, false otherwise
     */
    explicit operator bool() const { return m_function != nullptr; }

    /**
     * @brief Gets raw function pointer
     * @return Raw function pointer or nullptr
     */
    FnType get() const { return m_function; }

    /**
     * @brief Calls the resolved function with type checking via concepts
     *
     * @tparam Args Argument types (deduced)
     * @param args Arguments to pass to function
     * @return Function return value
     *
     * @throws std::runtime_error if function not resolved
     * @throws std::exception from invoked function
     *
     * @par Example
     * @code
     * if (auto creator = registry) {
     *     auto result = creator.call(arg1, arg2, arg3);
     * }
     * @endcode
     */
    template <typename... Args>
    requires std::invocable<FnType, Args...>
    auto call(Args&&... args) const
    {
        if (!m_function) {
            throw std::runtime_error("Function not loaded: " + m_symbolName);
        }

        try {
            return std::invoke(m_function, std::forward<Args>(args)...);
        } catch (const std::exception& e) {
            util::Logger::Error("Error calling plugin function {}: {}",
                              m_symbolName, e.what());
            throw;
        }
    }

  private:
    void* m_handle;
    std::string m_symbolName;
    FnType m_function;

    /**
     * @brief Resolves symbol from dynamic library
     * @return Function pointer if found, nullptr otherwise
     */
    FnType ResolveSymbol() const
    {
        void* ptr = nullptr;

#ifdef _WIN32
        ptr = GetProcAddress(static_cast<HMODULE>(m_handle),
                            m_symbolName.c_str());
#else
        // Clear any previous errors
        dlerror();
        ptr = dlsym(m_handle, m_symbolName.c_str());

        if (!ptr) {
            const char* error = dlerror();
            if (error) {
                util::Logger::Trace("dlsym error: {}", error);
            }
        }
#endif

        return ptr ? reinterpret_cast<FnType>(ptr) : nullptr;
    }
};

} // namespace plugin

#endif // PLUGIN_FUNCTION_REGISTRY_HPP
