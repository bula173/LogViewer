#pragma once

#include "mvc/IControler.hpp"
#include "ui/IEventsView.hpp"

#include <functional>
#include <string>
#include <vector>

namespace ui
{

class ISearchResultsViewObserver
{
  public:
    virtual ~ISearchResultsViewObserver() = default;
    virtual void OnSearchResultActivated(long eventId) = 0;
};

/** @brief Toolkit-agnostic contract for the search results panel. */
class ISearchResultsView
{
  public:
    virtual ~ISearchResultsView() = default;

    /** @brief Assigns an observer that receives activation callbacks. */
    virtual void SetObserver(ISearchResultsViewObserver* observer) = 0;

    /**
     * @brief Prepare the view for a new search.
     * Clears existing rows, rebuilds columns, and freezes visual updates.
     */
    virtual void BeginUpdate(const std::vector<std::string>& columns) = 0;

    /** @brief Append a single controller-produced search result row. */
    virtual void AppendResult(const mvc::SearchResultRow& row) = 0;

    /**
     * @brief Finalize the current update batch (thaws rendering).
     */
    virtual void EndUpdate() = 0;

    /** @brief Clear all rows without touching column configuration. */
    virtual void Clear() = 0;
};

/**
 * @brief Toolkit-neutral interface for the type filter checklist panel.
 */
class ITypeFilterView
{
  public:
    virtual ~ITypeFilterView() = default;

    /** @brief Register a callback invoked whenever selections change. */
    virtual void SetOnFilterChanged(std::function<void()> handler) = 0;

    /**
     * @brief Replace the entire list of types. Optionally auto-check.
     */
    virtual void ReplaceTypes(
        const std::vector<std::string>& types, bool checkedByDefault) = 0;

    /** @brief Show or hide the underlying control. */
    virtual void ShowControl(bool show) = 0;

    /** @brief Mark every entry as checked. */
    virtual void SelectAll() = 0;

    /** @brief Clear every selection. */
    virtual void DeselectAll() = 0;

    /** @brief Toggle the checked state of every entry. */
    virtual void InvertSelection() = 0;

    /** @brief Retrieve the labels for currently checked entries. */
    virtual std::vector<std::string> CheckedTypes() const = 0;

    /** @brief @return true when the list is empty. */
    virtual bool Empty() const = 0;
};

/**
 * @brief Toolkit-agnostic interface for the item/event details pane.
 */
class IItemDetailsView : public IEventDetailsView
{
  public:
    virtual ~IItemDetailsView() = default;

    /** @brief Show or hide the panel's visual control. */
    virtual void ShowControl(bool show) = 0;
};

} // namespace ui
