/**
 * @file PluginDependencyGraph.hpp
 * @brief Plugin dependency validation and topological sorting
 * @author LogViewer Development Team
 * @date 2026
 */

#ifndef PLUGIN_DEPENDENCY_GRAPH_HPP
#define PLUGIN_DEPENDENCY_GRAPH_HPP

#include "Result.hpp"
#include "Error.hpp"
#include "Logger.hpp"
#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace plugin {

/**
 * @class PluginDependencyGraph
 * @brief Manages plugin dependency relationships and validates for safe initialization
 *
 * Detects circular dependencies and provides topological sorting for safe
 * initialization and shutdown ordering.
 *
 * @par Features
 * - Cycle detection using DFS-based algorithm
 * - Topological sort for initialization order
 * - Reverse topological sort for shutdown order
 * - Missing dependency detection
 *
 * @par Usage Example
 * @code
 * PluginDependencyGraph graph;
 * graph.AddNode("plugin-a", {});
 * graph.AddNode("plugin-b", {"plugin-a"});
 * graph.AddNode("plugin-c", {"plugin-b", "plugin-a"});
 *
 * // Validate
 * if (auto result = graph.ValidateNoCycles(); result.isErr()) {
 *     Logger::Error("Circular dependency: {}", result.error().Message());
 *     return;
 * }
 *
 * // Get safe initialization order
 * auto order = graph.GetInitializationOrder();
 * for (const auto& id : order.unwrap()) {
 *     PluginManager::EnablePlugin(id);  // Guaranteed deps are loaded
 * }
 * @endcode
 */
class PluginDependencyGraph {
  public:
    /**
     * @struct DependencyNode
     * @brief Represents a plugin node in the dependency graph
     */
    struct DependencyNode {
        std::string pluginId;                ///< Plugin identifier
        std::vector<std::string> dependencies; ///< IDs of dependencies
    };

    /**
     * @brief Adds a node to the dependency graph
     * @param id Plugin identifier
     * @param dependencies List of dependency plugin IDs
     */
    void AddNode(const std::string& id, const std::vector<std::string>& dependencies)
    {
        m_nodes[id] = {id, dependencies};
        util::Logger::Trace("Added plugin node: {}, dependencies: {}",
                           id, dependencies.size());
    }

    /**
     * @brief Validates that no circular dependencies exist
     * @return Success with true, or Error with description
     *
     * @par Complexity
     * O(V + E) where V is nodes, E is edges
     */
    util::Result<bool, error::Error> ValidateNoCycles() const
    {
        std::set<std::string> visited, recursionStack;

        for (const auto& [id, _] : m_nodes) {
            if (!visited.count(id)) {
                if (HasCycle(id, visited, recursionStack)) {
                    std::string msg = "Circular dependency detected starting at: " + id;
                    util::Logger::Error("Plugin: {}", msg);
                    return util::Result<bool, error::Error>::Err(
                        error::Error(error::ErrorCode::RuntimeError, msg, false));
                }
            }
        }

        util::Logger::Debug("Plugin dependency graph validated - no cycles");
        return util::Result<bool, error::Error>::Ok(true);
    }

    /**
     * @brief Gets plugins in valid initialization order
     *
     * Plugins are ordered such that all dependencies appear before
     * the dependent plugin.
     *
     * @return Order for initialization, or error if cycles exist
     *
     * @par Example Result
     * If C depends on B, B depends on A:
     * Returns: [A, B, C]
     */
    util::Result<std::vector<std::string>, error::Error>
    GetInitializationOrder() const
    {
        if (auto result = ValidateNoCycles(); result.isErr()) {
            return util::Result<std::vector<std::string>, error::Error>::Err(
                result.error());
        }

        std::vector<std::string> order;
        std::set<std::string> visited;

        for (const auto& [id, _] : m_nodes) {
            TopologicalSort(id, visited, order);
        }

        // Don't reverse - DFS already produces correct order (deps before dependents)
        util::Logger::Debug("Plugin initialization order computed: {} plugins",
                           order.size());
        return util::Result<std::vector<std::string>, error::Error>::Ok(order);
    }

    /**
     * @brief Gets plugins in valid shutdown order
     *
     * Reverse of initialization order - shutdown dependents before dependencies.
     *
     * @return Order for shutdown, or error if cycles exist
     */
    util::Result<std::vector<std::string>, error::Error>
    GetShutdownOrder() const
    {
        auto order = GetInitializationOrder();
        if (order.isErr()) {
            return order;
        }

        auto shutdown = order.unwrap();
        std::reverse(shutdown.begin(), shutdown.end());

        util::Logger::Debug("Plugin shutdown order computed: {} plugins",
                           shutdown.size());
        return util::Result<std::vector<std::string>, error::Error>::Ok(shutdown);
    }

    /**
     * @brief Validates all dependencies are available
     *
     * @param availablePlugins Set of available plugin IDs
     * @return Missing dependency IDs, or empty if all satisfied
     */
    util::Result<std::vector<std::string>, error::Error>
    ValidateDependenciesAvailable(
        const std::set<std::string>& availablePlugins) const
    {
        std::vector<std::string> missing;

        for (const auto& [id, node] : m_nodes) {
            for (const auto& dep : node.dependencies) {
                if (!availablePlugins.count(dep)) {
                    missing.push_back(dep);
                    util::Logger::Warn(
                        "Plugin {} missing dependency: {}", id, dep);
                }
            }
        }

        if (!missing.empty()) {
            std::string list = FormatList(missing);
            std::string msg = "Missing plugin dependencies: " + list;
            util::Logger::Error("Plugin: {}", msg);
            return util::Result<std::vector<std::string>, error::Error>::Err(
                error::Error(error::ErrorCode::RuntimeError, msg, false));
        }

        util::Logger::Debug("All plugin dependencies validated");
        return util::Result<std::vector<std::string>, error::Error>::Ok(
            missing);
    }

    /**
     * @brief Gets dependencies for a specific plugin
     * @param id Plugin identifier
     * @return List of dependency IDs, or empty vector if not found
     */
    std::vector<std::string> GetDependencies(const std::string& id) const
    {
        auto it = m_nodes.find(id);
        return it != m_nodes.end() ? it->second.dependencies
                                   : std::vector<std::string>();
    }

    /**
     * @brief Gets number of nodes in graph
     * @return Node count
     */
    size_t GetNodeCount() const { return m_nodes.size(); }

  private:
    std::map<std::string, DependencyNode> m_nodes;

    /**
     * @brief Checks for cycle using depth-first search
     */
    bool HasCycle(const std::string& id, std::set<std::string>& visited,
                 std::set<std::string>& recursionStack) const
    {
        if (recursionStack.count(id)) {
            util::Logger::Warn("Circular dependency involving: {}", id);
            return true;
        }
        if (visited.count(id)) {
            return false;
        }

        visited.insert(id);
        recursionStack.insert(id);

        auto it = m_nodes.find(id);
        if (it != m_nodes.end()) {
            for (const auto& dep : it->second.dependencies) {
                if (HasCycle(dep, visited, recursionStack)) {
                    return true;
                }
            }
        }

        recursionStack.erase(id);
        return false;
    }

    /**
     * @brief Topological sort using DFS
     */
    void TopologicalSort(const std::string& id,
                         std::set<std::string>& visited,
                         std::vector<std::string>& order) const
    {
        if (visited.count(id)) {
            return;
        }
        visited.insert(id);

        auto it = m_nodes.find(id);
        if (it != m_nodes.end()) {
            for (const auto& dep : it->second.dependencies) {
                TopologicalSort(dep, visited, order);
            }
        }

        order.push_back(id);
    }

    /**
     * @brief Formats vector of strings for logging
     */
    static std::string FormatList(const std::vector<std::string>& items)
    {
        std::string result = "[";
        for (size_t i = 0; i < items.size(); ++i) {
            if (i > 0) result += ", ";
            result += items[i];
        }
        result += "]";
        return result;
    }
};

} // namespace plugin

#endif // PLUGIN_DEPENDENCY_GRAPH_HPP
