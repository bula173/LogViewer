# Design Flaws: Feature Interaction Map

## Visual: Where Multiple Features Break When Combined

```
┌──────────────────────────────────────────────────────────────────────────┐
│                                                                          │
│                        EVENT CONTAINER                                  │
│                    (grows during file load)                             │
│                                                                          │
│  Events: [0][1][2]...[500]──────→  Parser adds 50 more events          │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
         ▲              ▲              ▲                ▲
         │              │              │                │
    ┌────┴────┐    ┌────┴────┐   ┌────┴─────┐     ┌───┴────┐
    │          │    │         │   │          │     │        │
    │  Search  │    │ Filter  │   │ Dashboard│     │ Export │
    │  Counts  │    │ Indices │   │ Stats    │     │ Report │
    │          │    │         │   │          │     │        │
    └────┬─────┘    └────┬────┘   └────┬─────┘     └───┬────┘
         │               │             │               │
         │               │             │               │
         │  BUG #3:      │  BUG #4:    │   BUG #2:    │
         │  No sync      │  Indices    │   No filtering  BUG #1:
         │  during       │  become     │   awareness  Stale indices
         │  iteration    │  invalid    │              during export
         │               │             │
         └───────┬───────┴─────────────┴───────────────┘
                 │
                 ▼
          USER CONFUSION
          ──────────────
          
          "Dashboard shows 1000 events"
          "Export shows 100 events"  
          "Report matches: 48"
          "Search bar shows: 0 matches"
          
          ↑ WHICH IS CORRECT?!
```

---

## The 5-Step Cascade: How Features Break Together

### Scenario: User performs typical workflow

```
STEP 1: USER LOADS FILE (500 events)
═════════════════════════════════════════════════════════════════════

    EventsContainer        Dashboard          Search Bar         Table
    ─────────────          ────────────       ─────────          ─────
    Size: 500             Shows: 500         Counts: 0/500      Displays: 500
                          Errors: 247        (empty initially)   
                          Warnings: 512

    ✓ All systems in sync


STEP 2: USER APPLIES TIME-RANGE FILTER (→ 100 events)
═════════════════════════════════════════════════════════════════════

    Filtering logic:
    1. EventsTableModel calculates filtered indices for matching events
    2. m_filteredIndices = [0, 10, 20, 30, ..., 490]  (100 items)
    3. Table view refreshed, shows 100 events

    BUT:

    EventsContainer        Dashboard          Search Bar         Table
    ─────────────          ────────────       ─────────          ─────
    Size: 500             Shows: 500 ✗      Counts: 0/500 ✗    Displays: 100 ✓
                          Errors: 247 ✗     (didn't update)
                          Warnings: 512 ✗

    ✗ BROKEN: Dashboard showing unfiltered stats!
    ✗ BROKEN: Search still counting all events!


STEP 3: USER TYPES "ERROR" IN SEARCH BAR
═════════════════════════════════════════════════════════════════════

    Event flow:
    1. User types "E"
    2. OnSearchTextChanged() fires immediately
       → Emits SearchChanged("E")
       → EventsTableModel::SetSearchTerm("E") called
       → Rebuilds search matches (respects filtering!)
       → Only highlights in filtered view (48 matches in 100 events)
    
    3. UnifiedSearchBar starts 300ms debounce timer
       → CountMatches("E") fires later
       → Counts ALL events (98 matches in 500 events)

    EventsContainer        Dashboard          Search Bar         Table
    ─────────────          ────────────       ─────────          ─────
    Size: 500             Shows: 500         Counts: 98/500 ✗   Highlights: 48
                          Errors: 247        (all events)        matches in 100 ✗
                          Warnings: 512

    ✗ BROKEN: Search count doesn't match table highlighting!
    ✗ BROKEN: User sees "98 matches" but only 48 visible!


STEP 4: PARSER THREAD ADDS EVENTS (500 → 550) MID-WORKFLOW
═════════════════════════════════════════════════════════════════════

    While user is examining results, background file parser continues:

    Race conditions triggered:

    RACE #1: CountMatches() iterating
    ─────────────────────────────────
    for (size_t i = 0; i < 500; ++i)  // Size was 500
        GetEvent(i)                    // ← But container now 550!
        
    RACE #2: Filter indices become stale
    ─────────────────────────────────────
    m_filteredIndices calculated for 500 events
    But 50 NEW events matching filter might exist!
    New events NOT in m_filteredIndices


    EventsContainer        Dashboard          Search Bar         Table
    ─────────────          ────────────       ─────────          ─────
    Size: 550             Shows: 500         Counts: 98/550 ✓   Displays: 100 ✓
                          Errors: 247        (recalculated)      (stale indices)
                          Warnings: 512

    ✗ BROKEN: New events not included in filter!
    ✗ BROKEN: Table showing results for 100 items (old filter)
              not including new matching events!


STEP 5: USER CLICKS "GENERATE REPORT"
═════════════════════════════════════════════════════════════════════

    Report generation flow:
    1. GetRowsToExport() called at T=0ms
       → Returns filtered indices: [0, 10, 20, ...] (100 items)
    
    2. [Parser continues adding events, now 600 total]
    
    3. ReportGenerator starts with indices from T=0ms
       → Tries to generate report for [0, 10, 20, ...]
       → Gets 100 events
       
    4. BUT: Filter was based on 550 events, not 600!
       → Report missing NEW events
       → Report statistics wrong
       → Report size: 100 events
    
    Meanwhile:
    5. Dashboard still shows: 500 (original load count)
    6. Search bar shows: 98/600 (after debounce fired)
    7. Table shows: 100 (stale filter)
    8. Report says: 100 events analyzed

    EventsContainer        Dashboard          Search Bar         Table
    ─────────────          ────────────       ─────────          ─────
    Size: 600             Shows: 500 ✗       Counts: 98/600     Displays: 100 ✗
                          Errors: 247 ✗      (time T=300ms)
                          Warnings: 512 ✗

    ✗ BROKEN: Dashboard = 500
    ✗ BROKEN: Search = 600
    ✗ BROKEN: Table = 100 (but not the right 100!)
    ✗ BROKEN: Report = 100 (but different events!)


FINAL STATE: USER SEES THIS
═════════════════════════════════════════════════════════════════════

    Dashboard Panel:          Table View:           Search Bar:
    ┌────────────────┐       ┌──────────────┐       ┌─────────────┐
    │ Total: 500     │       │ 100 visible  │       │ E            │
    │ Errors: 247    │       │ [filtered]   │       │ Matches: 98  │
    │ Warnings: 512  │       │ Highlights:  │       │ of 600       │
    └────────────────┘       │ 48 events    │       └─────────────┘
                             └──────────────┘

    ↑ Same file, THREE different numbers!
    ↑ User: "Wait, which is correct? 500? 600? 100?"

    Report Generated:
    ┌──────────────────────────────┐
    │ Report: 100 events analyzed  │
    │ ERROR: 23                    │
    │ WARNING: 45                  │
    │ INFO: 32                     │
    └──────────────────────────────┘

    ↑ Report says 100, but dashboard showed 500!
    ↑ Report error count ≠ dashboard error count!
```

---

## Root Causes: The Design Flaws

```
┌─────────────────────────────────────────────────────────────────┐
│ FLAW #1: No State Ownership                                     │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│ Dashboard "owns" full stats?      NO CLEAR OWNER              │
│ Search bar "owns" match count?    INDEPENDENT CALCULATION     │
│ Table "owns" filtered view?       SEPARATE FILTER LOGIC       │
│ Report "owns" export indices?     USES STALE DATA             │
│                                                                 │
│ → No single source of truth                                    │
│ → Multiple competing calculations                             │
│ → No synchronization mechanism                                │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│ FLAW #2: No Event Propagation                                   │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│ When filter changes:                                            │
│   • EventsTableView knows         ✓                            │
│   • EventsTableModel knows        ✓                            │
│   • DashboardPanel knows?         ✗ NO SIGNAL                 │
│   • UnifiedSearchBar knows?       ✗ NO SIGNAL                 │
│                                                                 │
│ → Components operate in isolation                             │
│ → Changes not propagated                                       │
│ → Stale state accumulates                                      │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│ FLAW #3: Race Conditions + Concurrent Modifications             │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│ Parser thread:  Add events → Container grows                  │
│ UI thread:      Read indices → May become invalid             │
│                                                                 │
│ No synchronization:                                             │
│   • No mutex around GetEvent()                                 │
│   • No atomic filter snapshot                                  │
│   • Debounce creates timing window                             │
│                                                                 │
│ Result: Silent data corruption                                │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│ FLAW #4: Time-Dependent Correctness                             │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│ Same user action produces different results depending on:       │
│   • Timing of parser thread                                    │
│   • Debounce timer firing                                      │
│   • Filter recalculation                                       │
│   • Report generation start time                               │
│                                                                 │
│ → Non-deterministic behavior                                   │
│ → Hard to debug (Heisenbug)                                    │
│ → Unreproducible crashes                                       │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## How to Fix: Architecture Changes

```
BEFORE (Current - Broken):
─────────────────────────

    EventsContainer
         │
    ┌────┼────┬─────────┬──────────┐
    │    │    │         │          │
    ▼    ▼    ▼         ▼          ▼
  Search Filter Dashboard Report  Table
  (separate calculations, no coordination)


AFTER (Fixed - Unified):
────────────────────────

    EventsContainer
         │
         ├─→ FilterManager
         │       │
         │    [Filtered Indices Cache]
         │       │
         ├─→ emit: FilteredIndicesChanged
         │
    ┌────┴────────┬──────────────┬──────────┐
    │             │              │          │
    ▼             ▼              ▼          ▼
  Dashboard   Report       Table       Search
  (listens)  (uses cache)  (uses)   (respects filter)
```

**Key Changes Needed:**

1. **Unified FilterState**
   - Single source of truth for which events are visible
   - All components read from it
   - Only FilterManager updates it

2. **Event Propagation**
   - FilterChanged() signal → all listeners
   - EventCountChanged() signal → Dashboard, SearchBar
   - ClearFilter() → propagate to all components

3. **Thread Synchronization**
   - Mutex around EventsContainer access
   - Copy filter indices atomically
   - Validate indices before use

4. **Dashboard Filtering Awareness**
   - Listen to FilteredIndicesChanged
   - Show "Showing X of Y" when filter active
   - Update stats immediately when filter changes

5. **Search Unification**
   - Single SearchMatcher used everywhere
   - Respects filtering by default
   - Consistent results across UI

---

## Testing: Verify The Fixes

```cpp
// Test #1: Dashboard reflects filtering
TEST(DashboardPanelTest, StatsChangeWhenFilterApplied) {
    // Load 1000 events
    // Apply filter → 100 visible
    // Dashboard should show "100 of 1000"
    EXPECT_EQ(dashboard->GetVisibleCount(), 100);
    EXPECT_EQ(dashboard->GetTotalCount(), 1000);
}

// Test #2: Search respects filtering  
TEST(SearchTest, MatchCountRespectsFiler) {
    // Load events, apply filter
    // Search should only count in filtered view
    int matches = searchBar->CountMatches("ERROR");
    EXPECT_EQ(matches, filtered_error_count);  // Not all_error_count
}

// Test #3: Concurrent load doesn't break filtering
TEST(ConcurrencyTest, FilterValidDuringConcurrentLoad) {
    // Start with 100 events (filtered to 50)
    // Parser adds 100 more events
    // Filter should still work, possibly including new matches
    EXPECT_NO_CRASH();
    int matches = searchBar->CountMatches("query");
    EXPECT_GE(matches, 0);  // Some valid count
}

// Test #4: Report uses correct indices
TEST(ReportTest, ExportUsesCurrentFilteredIndices) {
    // Apply filter, generate report
    // Report should contain exactly filtered events
    EXPECT_EQ(report.event_count(), filtered_count);
    for (auto& event : report.events()) {
        EXPECT_TRUE(event_matches_filter(event));
    }
}

// Test #5: Dashboard + Export consistency
TEST(IntegrationTest, DashboardExportConsistency) {
    int dashboard_count = dashboard->GetVisibleCount();
    int export_count = GetRowsToExport().size();
    int report_count = generated_report.event_count();
    
    // All should be the same!
    EXPECT_EQ(dashboard_count, export_count);
    EXPECT_EQ(export_count, report_count);
}
```

---

**Conclusion:**

The current v1.11.0 implementation treats Dashboard, Search, Filtering, and Export as independent components. When they interact in real workflows, they produce inconsistent, confusing results.

The fix requires:
1. **Unified state management** (single source of truth)
2. **Event propagation** (components listen to changes)
3. **Thread synchronization** (safe concurrent access)
4. **Comprehensive testing** (verify interactions work)

Estimated effort: **6-10 days** for complete redesign and testing.

