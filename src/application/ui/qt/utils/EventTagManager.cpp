#include "EventTagManager.hpp"
#include "Config.hpp"
#include "Logger.hpp"

#include <filesystem>
#include <fstream>
#include <chrono>

namespace ui::qt::utils
{

using json = nlohmann::json;

EventTagManager& EventTagManager::getInstance()
{
    static EventTagManager instance;
    return instance;
}

void EventTagManager::addTagType(const Tag& tag)
{
    // Check if tag already exists
    for (const auto& t : m_tags)
    {
        if (t.name == tag.name)
            return;
    }

    m_tags.push_back(tag);
    util::Logger::Info("[EventTagManager] Added tag type: {}", tag.name);
}

void EventTagManager::removeTagType(const std::string& tagName)
{
    auto it = std::find_if(m_tags.begin(), m_tags.end(),
        [&tagName](const Tag& t) { return t.name == tagName; });

    if (it != m_tags.end())
    {
        m_tags.erase(it);
        util::Logger::Info("[EventTagManager] Removed tag type: {}", tagName);
    }
}

void EventTagManager::tagEvent(int eventId, const std::string& tagName)
{
    // Ensure tag type exists
    bool tagExists = false;
    for (const auto& t : m_tags)
    {
        if (t.name == tagName)
        {
            tagExists = true;
            break;
        }
    }

    if (!tagExists)
        return;

    auto it = m_annotations.find(eventId);
    if (it == m_annotations.end())
    {
        EventAnnotation annotation;
        annotation.eventId = eventId;
        annotation.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
        m_annotations[eventId] = annotation;
    }

    m_annotations[eventId].tagNames.insert(tagName);

    util::Logger::Debug("[EventTagManager] Tagged event {} with '{}'", eventId, tagName);
}

void EventTagManager::untagEvent(int eventId, const std::string& tagName)
{
    auto it = m_annotations.find(eventId);
    if (it != m_annotations.end())
    {
        it->second.tagNames.erase(tagName);

        // Remove entry if no tags or notes remain
        if (it->second.tagNames.empty() && it->second.notes.empty())
        {
            m_annotations.erase(it);
        }

        util::Logger::Debug("[EventTagManager] Untagged event {} from '{}'", eventId, tagName);
    }
}

const std::set<std::string>& EventTagManager::getEventTags(int eventId) const
{
    static const std::set<std::string> empty;
    auto it = m_annotations.find(eventId);
    if (it != m_annotations.end())
        return it->second.tagNames;
    return empty;
}

const std::string& EventTagManager::getEventAnnotation(int eventId) const
{
    static const std::string empty;
    auto it = m_annotations.find(eventId);
    if (it != m_annotations.end())
        return it->second.notes;
    return empty;
}

void EventTagManager::annotateEvent(int eventId, const std::string& notes)
{
    auto it = m_annotations.find(eventId);
    if (it == m_annotations.end())
    {
        EventAnnotation annotation;
        annotation.eventId = eventId;
        m_annotations[eventId] = annotation;
    }

    m_annotations[eventId].notes = notes;
    m_annotations[eventId].timestamp = std::chrono::system_clock::now().time_since_epoch().count();

    util::Logger::Debug("[EventTagManager] Annotated event {} with {} chars", eventId, notes.length());
}

std::vector<int> EventTagManager::getEventsWithTag(const std::string& tagName) const
{
    std::vector<int> result;
    for (const auto& [eventId, annotation] : m_annotations)
    {
        if (annotation.tagNames.count(tagName) > 0)
        {
            result.push_back(eventId);
        }
    }
    return result;
}

void EventTagManager::saveTags()
{
    try
    {
        const auto& configPath = config::GetConfig().GetConfigFilePath();
        const auto appDir = std::filesystem::path(configPath).parent_path();
        m_tagsFilePath = (appDir / "tags.json").string();

        json j;
        j["version"] = "1.0";

        // Save tag types
        json tagsArray = json::array();
        for (const auto& tag : m_tags)
        {
            tagsArray.push_back({
                {"name", tag.name},
                {"color", tag.color},
                {"description", tag.description}
            });
        }
        j["tagTypes"] = tagsArray;

        // Save annotations
        json annotationsObj = json::object();
        for (const auto& [eventId, annotation] : m_annotations)
        {
            json annJson;
            annJson["tags"] = json(annotation.tagNames);
            annJson["notes"] = annotation.notes;
            annJson["timestamp"] = annotation.timestamp;
            annotationsObj[std::to_string(eventId)] = annJson;
        }
        j["annotations"] = annotationsObj;

        std::ofstream file(m_tagsFilePath);
        if (file.is_open())
        {
            file << j.dump(2);
            file.close();
            util::Logger::Info("[EventTagManager] Saved {} tags and {} annotations",
                m_tags.size(), m_annotations.size());
        }
    }
    catch (const std::exception& e)
    {
        util::Logger::Error("[EventTagManager] Failed to save tags: {}", e.what());
    }
}

void EventTagManager::loadTags()
{
    try
    {
        const auto& configPath = config::GetConfig().GetConfigFilePath();
        const auto appDir = std::filesystem::path(configPath).parent_path();
        m_tagsFilePath = (appDir / "tags.json").string();

        if (!std::filesystem::exists(m_tagsFilePath))
            return;

        std::ifstream file(m_tagsFilePath);
        if (!file.is_open())
            return;

        json j;
        file >> j;
        file.close();

        // Load tag types
        if (j.contains("tagTypes") && j["tagTypes"].is_array())
        {
            for (const auto& tagJson : j["tagTypes"])
            {
                Tag tag;
                tag.name = tagJson.value("name", "");
                tag.color = tagJson.value("color", "#000000");
                tag.description = tagJson.value("description", "");
                if (!tag.name.empty())
                {
                    m_tags.push_back(tag);
                }
            }
        }

        // Load annotations
        if (j.contains("annotations") && j["annotations"].is_object())
        {
            for (const auto& [eventIdStr, annJson] : j["annotations"].items())
            {
                try
                {
                    int eventId = std::stoi(eventIdStr);
                    EventAnnotation ann;
                    ann.eventId = eventId;
                    ann.notes = annJson.value("notes", "");
                    ann.timestamp = annJson.value("timestamp", 0LL);

                    if (annJson.contains("tags") && annJson["tags"].is_array())
                    {
                        ann.tagNames = annJson["tags"].get<std::set<std::string>>();
                    }

                    m_annotations[eventId] = ann;
                }
                catch (const std::exception&)
                {
                    continue;
                }
            }
        }

        util::Logger::Info("[EventTagManager] Loaded {} tags and {} annotations",
            m_tags.size(), m_annotations.size());
    }
    catch (const std::exception& e)
    {
        util::Logger::Error("[EventTagManager] Failed to load tags: {}", e.what());
    }
}

void EventTagManager::clearTags()
{
    m_tags.clear();
    m_annotations.clear();
    util::Logger::Info("[EventTagManager] Cleared all tags and annotations");
}

}  // namespace ui::qt::utils
