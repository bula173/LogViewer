# Event Merge Strategy Update

## Overview

Updated the event merging strategy in LogViewer to handle timestamp ordering and ID preservation when merging events from multiple sources.

## Changes Made

### 1. **Enhanced Merge Timestamp Comparison** 
   **File**: `src/application/db/EventsContainer.cpp`
   
   The `isBefore()` comparator in `MergeEvents()` now:
   - **First**: Compares timestamps (primary sort key)
   - **Then**: If timestamps are equal, compares event IDs (secondary sort key/tiebreaker)
   
   ```cpp
   if (timestampA != timestampB)
       return timestampA < timestampB;
   
   // If timestamps are equal, use event ID as tiebreaker
   return a.getId() < b.getId();
   ```

### 2. **Original ID Preservation**
   **Files**: 
   - `src/application/db/LogEvent.hpp` (added method declaration)
   - `src/application/db/LogEvent.cpp` (implementation)
   
   Added `SetOriginalId(int originalId)` method to LogEvent that:
   - Stores the original event ID as a special `original_id` field in the event data
   - Allows users to see both the original and merged event IDs
   - Removes any existing `original_id` field to avoid duplicates
   - Rebuilds the lookup index after adding the field
   
   **Usage**: When merging events from different sources, the original event ID is automatically preserved as an event field.

### 3. **Event Merge Flow**
   
   When `MergeEvents()` is called:
   1. Events from both containers are sorted by timestamp + event ID
   2. Events from the "other" container are marked with `newAlias` as source
   3. Events from "other" container have their original IDs stored via `SetOriginalId()`
   4. Final merged list preserves chronological order with consistent ID handling

## Problem Solved

### Before
- When merging logs from two sources (e.g., two log files), events with the same ID could have different meanings
- No way to identify the original ID from the source container after merge
- Confusing duplicate IDs in the merged result

### After
- Original event IDs are preserved as `original_id` field in the event data
- Users can see: **Event ID** (new unique ID after merge) + **Original ID** (ID from source)
- Events are sorted correctly by timestamp, with IDs used as tiebreaker when timestamps match
- Events show both their new and legacy IDs in the UI

## Example

**Before Merge:**
- Log A: Event ID=1, timestamp="2025-01-07T10:00:00"
- Log B: Event ID=1, timestamp="2025-01-07T10:00:05"

**After Merge:**
- Event 1: ID=1 (Log A), timestamp="2025-01-07T10:00:00", original_id=1
- Event 2: ID=1 (Log B), timestamp="2025-01-07T10:00:05", original_id=1 ← Now has original_id field

**Display in UI:**
- Event 1: ID: 1 | Original ID: 1 | Source: Log A | Time: 10:00:00
- Event 2: ID: 1 | Original ID: 1 | Source: Log B | Time: 10:00:05

## Configuration

No configuration needed - this is automatic when merging events:
```cpp
container.MergeEvents(other, "existing_log.xml", "new_log.xml", "timestamp");
```

The merge automatically:
1. Sorts by timestamp
2. Uses event ID as tiebreaker  
3. Stores original IDs for events from 'other'
4. Maintains source information

## Benefits

✅ Clear identification of event origins  
✅ Prevents ID collision confusion  
✅ Preserves chronological order  
✅ Transparent handling of multi-source logs  
✅ No additional user configuration required  

## Testing

The merge strategy preserves:
- Chronological order (primary by timestamp, secondary by ID)
- Event source information
- Original event IDs for reference
- Lookup performance with rebuilt indices
