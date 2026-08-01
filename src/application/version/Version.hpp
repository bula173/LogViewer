/**
 * @file version.h
 * @brief Version information and utilities for the LogViewer application.
 * @author LogViewer Development Team
 * @date 2025
 */

#pragma once
#include <string>
#include <compare>

/**
 * @namespace Version
 * @brief Contains version-related structures and utilities.
 */
namespace Version
{

/**
 * @struct Version
 * @brief Represents application version information with semantic versioning
 * support.
 *
 * This structure encapsulates version information including major, minor, and
 * patch numbers following semantic versioning conventions, along with build
 * metadata.
 *
 * @see https://semver.org/ for semantic versioning specification
 */
struct Version
{
    int major {0}; ///< Major version number (breaking changes)
    int minor {0}; ///< Minor version number (new features, backward compatible)
    int patch {0}; ///< Patch version number (bug fixes, backward compatible)
    std::string type {
        ""}; ///< Version type (e.g., "alpha", "beta", "rc", "stable")
    std::string datetime {""}; ///< Build date and time in ISO format
    std::string machine {""};  ///< Build machine identifier

    /**
     * @brief Converts version to a single numeric representation.
     *
     * Returns version in one big number with format:
     * MMMMmmmmpppp (M-Major, m-minor, p-patch)
     *
     * Example: Version 1.2.3 becomes 100020003 (leading zeroes are not
     * displayed)
     *
     * @return long long integer with encoded version information
     * @note This method is useful for version comparison operations
     */
    long long asNumber() const;

    /**
     * @brief Returns short version string representation.
     *
     * Format: "Major.Minor.Patch VersionType"
     * Example: "1.2.3 beta"
     *
     * @return std::string containing the short version representation
     */
    std::string asShortStr() const;

    /**
     * @brief Returns detailed version string representation.
     *
     * Includes additional build information such as build date and machine.
     *
     * @return std::string containing the detailed version representation
     */
    std::string asLongStr() const;

    /**
     * @brief Three-way comparison operator (spaceship operator)
     *
     * Provides complete version ordering with semantic versioning rules.
     * Compiler automatically generates <, >, <=, >=, != from this and operator==.
     *
     * Comparison order: major → minor → patch → type
     * Pre-release versions (alpha, beta, rc) are considered less than stable.
     *
     * @param other The version to compare against
     * @return Strong ordering comparison result
     */
    auto operator<=>(const Version& other) const;

    /// Equality operator (compares numeric version; compiler generates != from this)
    bool operator==(const Version& other) const;

    /// Creates empty Version object
    Version();

    /**
     * Converts number to version object.
     * @param version - number in format MMMMmmmmpppp (M-Major, m-minor,
     * p-patch)
     *
     * @return Version object
     */
    Version(long long version);
};

/** Get current version
 * @return const Version& - current version of the application
 */
const Version& current();

} // namespace Version
