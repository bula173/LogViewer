#pragma once

#include <string>
#include <vector>
#include <set>
#include <map>
#include <nlohmann/json.hpp>

namespace ui::qt::utils
{

/**
 * @brief Manages tags/annotations for log events.
 *
 * Allows users to:
 * - Tag events with custom labels
 * - Add notes/annotations
 * - Color-code tagged events
 * - Filter by tags
 * - Persist tags across sessions
 */
class EventTagManager
{
  public:
    struct Tag
    {
        std::string name;
        std::string color;     // Hex color code
        std::string description;
    };

    struct EventAnnotation
    {
        int eventId;
        std::set<std::string> tagNames;  // Tag names applied to this event
        std::string notes;
        int64_t timestamp;  // When annotation was created
    };

    static EventTagManager& getInstance();

    /// Add a tag type (e.g., "Error", "Warning", "Important")
    void addTagType(const Tag& tag);

    /// Remove a tag type
    void removeTagType(const std::string& tagName);

    /// Get all tag types
    const std::vector<Tag>& tagTypes() const { return m_tags; }

    /// Tag an event
    void tagEvent(int eventId, const std::string& tagName);

    /// Untag an event
    void untagEvent(int eventId, const std::string& tagName);

    /// Add annotation/notes to event
    void annotateEvent(int eventId, const std::string& notes);

    /// Get tags for an event
    const std::set<std::string>& getEventTags(int eventId) const;

    /// Get annotation for an event
    const std::string& getEventAnnotation(int eventId) const;

    /// Get all annotated events
    const std::map<int, EventAnnotation>& allAnnotations() const { return m_annotations; }

    /// Filter events by tag
    std::vector<int> getEventsWithTag(const std::string& tagName) const;

    /// Save tags to file
    void saveTags();

    /// Load tags from file
    void loadTags();

    /// Clear all tags
    void clearTags();

  private:
    EventTagManager() = default;

    std::vector<Tag> m_tags;
    std::map<int, EventAnnotation> m_annotations;
    std::string m_tagsFilePath;
};

}  // namespace ui::qt::utils
