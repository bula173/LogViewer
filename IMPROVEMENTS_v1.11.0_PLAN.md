# v1.11.0 Improvements Implementation Plan

Based on comprehensive code review findings, here's the prioritized plan for improvements.

## Status: 🔴 In Progress

---

## 🔴 Phase 1: CRITICAL FIXES (6.5 hours)

### 1. XSS Vulnerability in Report Generation
**Priority:** CRITICAL (Security)  
**File:** `src/application/ui/qt/utils/ReportGenerator.cpp`  
**Effort:** 30 minutes  
**Status:** TODO

**Issue:** Event data not escaped in HTML/Markdown reports  
**Threat:** Exported reports can execute arbitrary JavaScript

**Implementation:**
- [ ] Add HTML escaping using Qt::escape() for HTML output
- [ ] Add comprehensive Markdown escaping (backslash, pipe, newlines)
- [ ] Add JSON escaping for JSON output
- [ ] Add unit tests for XSS prevention
- [ ] Test with malicious payloads

---

### 2. Dashboard Filtering Awareness
**Priority:** CRITICAL (Data Integrity)  
**File:** `src/application/ui/qt/panels/DashboardPanel.cpp`  
**Effort:** 2 hours  
**Status:** TODO

**Issue:** Dashboard shows all-event statistics regardless of active filter  
**Impact:** User confusion, misleading analysis, inconsistent numbers

**Implementation:**
- [ ] Add `OnFilteredIndicesChanged()` slot to DashboardPanel
- [ ] Add filtering state tracking (`m_filteringActive`, `m_filteredIndices`)
- [ ] Update `UpdateEventStats()` to use filtered indices when active
- [ ] Add visual indicator: "📊 Showing X of Y" when filtered
- [ ] Connect FilteredIndicesChanged signal in MainWindow
- [ ] Add unit tests for filtered stats calculation

---

### 3. Race Condition: Search During File Load
**Priority:** CRITICAL (Thread Safety)  
**File:** `src/application/ui/qt/panels/UnifiedSearchBar.cpp`  
**Effort:** 1 hour  
**Status:** TODO

**Issue:** `CountMatches()` iterates without synchronization  
**Impact:** Crashes, incorrect counts, data loss

**Implementation:**
- [ ] Add per-event error handling with try-catch
- [ ] Cache EventsContainer size before iteration
- [ ] Check size on each iteration before GetEvent()
- [ ] Log errors instead of silently skipping
- [ ] Add integration test for search during concurrent load
- [ ] Consider adding mutex if needed

---

### 4. Stale Filter Indices During Export
**Priority:** CRITICAL (Data Integrity)  
**File:** `src/application/ui/qt/MainWindow.cpp`  
**Effort:** 1 hour  
**Status:** TODO

**Issue:** Indices become invalid when EventsContainer grows during export  
**Impact:** Reports contain wrong events or crash

**Implementation:**
- [ ] Copy filtered indices atomically in GetRowsToExport()
- [ ] Validate each index before using
- [ ] Log and skip invalid indices
- [ ] Add bounds checking in ReportGenerator::generateReport()
- [ ] Add integration test for export during file load

---

### 5. Malloc Memory Leak in Plugin Bridge
**Priority:** CRITICAL (Memory Safety)  
**File:** `src/application/ui/qt/MainWindow.cpp` lines 135-137  
**Effort:** 30 minutes  
**Status:** TODO

**Issue:** Unsafe malloc/free in plugin bridge  
**Impact:** Memory accumulation over time

**Implementation:**
- [ ] Replace malloc with Qt::qstrdup() (Qt memory pool)
- [ ] Update plugin bridge documentation
- [ ] Test plugin memory usage
- [ ] Verify no memory leaks with valgrind/ASAN

---

## 🟠 Phase 2: HIGH PRIORITY FIXES (4 hours)

### 6. Unify Search Implementation
**Priority:** HIGH (Code Quality)  
**File:** Create new `SearchMatcher.hpp/cpp`  
**Effort:** 3 hours  
**Status:** TODO

**Issue:** 3 incompatible search implementations  
**Impact:** Inconsistent results, user confusion

**Implementation:**
- [ ] Create SearchMatcher utility class
- [ ] Support Substring, Regex, Advanced modes
- [ ] Use in UnifiedSearchBar for counting
- [ ] Use in EventsTableModel for highlighting
- [ ] Validate behavior consistency across all modes
- [ ] Add comprehensive tests

---

### 7. Replace Q_ASSERT with Proper Error Handling
**Priority:** HIGH (Error Handling)  
**File:** `src/application/ui/qt/panels/UnifiedSearchBar.cpp`  
**Effort:** 1 hour  
**Status:** TODO

**Issue:** Q_ASSERT is no-op in Release builds  
**Impact:** Silent crashes under memory pressure

**Implementation:**
- [ ] Replace Q_ASSERT_X with explicit checks
- [ ] Throw exceptions on widget allocation failure
- [ ] Propagate errors to caller
- [ ] Add try-catch in constructor
- [ ] Test in low-memory conditions

---

### 8. Optimize O(n²) Unique Actor Detection
**Priority:** HIGH (Performance)  
**File:** `src/application/ui/qt/utils/ReportGenerator.cpp` line 258  
**Effort:** 30 minutes  
**Status:** TODO

**Issue:** std::find in loop = O(n²) performance  
**Impact:** 100K events → 5-10 seconds (can be 50-100ms)

**Implementation:**
- [ ] Replace std::vector with std::set for deduplication
- [ ] Test performance: measure before/after
- [ ] Verify correctness: same results, different order
- [ ] Consider using QSet for Qt integration

---

## 🟡 Phase 3: MEDIUM PRIORITY FIXES (2 hours)

### 9. Dashboard Not Updated on Filter Changes
**Priority:** MEDIUM (Logical Error)  
**File:** `src/application/ui/qt/MainWindow.cpp` (signal connections)  
**Effort:** 1 hour  
**Status:** TODO

**Implementation:**
- [ ] Connect FilteredIndicesChanged signal
- [ ] Trigger Dashboard::OnFilteredIndicesChanged()
- [ ] Ensure Dashboard updates immediately

---

### 10. Complete Markdown Escaping
**Priority:** MEDIUM (Data Integrity)  
**File:** `src/application/ui/qt/utils/ReportGenerator.cpp`  
**Effort:** 30 minutes  
**Status:** TODO

**Issue:** Only pipe character escaped, not newlines/backslash  
**Impact:** Corrupted Markdown tables

**Implementation:**
- [ ] Create EscapeMarkdownTableCell() utility function
- [ ] Escape: backslash, pipe, newline, HTML entities
- [ ] Test with problematic characters
- [ ] Verify table formatting

---

## 📊 Implementation Checklist

### Phase 1: Critical Fixes
- [ ] Fix #1: XSS Escaping (30 min)
- [ ] Fix #2: Dashboard Filtering (2 hours)
- [ ] Fix #3: Race Condition (1 hour)
- [ ] Fix #4: Stale Indices (1 hour)
- [ ] Fix #5: Malloc Leak (30 min)
- **Subtotal: 6.5 hours**

### Phase 2: High Priority
- [ ] Fix #6: Search Unification (3 hours)
- [ ] Fix #7: Q_ASSERT → Error Handling (1 hour)
- [ ] Fix #8: O(n²) → O(n log n) (30 min)
- **Subtotal: 4.5 hours**

### Phase 3: Medium Priority
- [ ] Fix #9: Dashboard Filter Signals (1 hour)
- [ ] Fix #10: Markdown Escaping (30 min)
- **Subtotal: 1.5 hours**

### Testing & Review
- [ ] Unit tests for all fixes
- [ ] Integration tests for interactions
- [ ] Code review of all changes
- [ ] Final validation
- **Subtotal: 3-4 hours**

---

## Expected Timeline

**Total Estimated Effort:** 15-16 hours focused work

**Recommended Schedule:**
- Day 1: Phase 1 Critical Fixes (6.5 hours)
- Day 2: Phase 2 High Priority Fixes (4.5 hours)
- Day 3: Phase 3 Medium Priority + Testing (5 hours)
- **Expected Release Ready:** ~3 days

---

## Quality Gates Before Release

- [x] All critical bugs fixed
- [ ] All high-priority bugs fixed
- [ ] No regression in existing tests
- [ ] New tests for all fixes passing (100%)
- [ ] Performance tests passing
- [ ] Manual QA verification complete
- [ ] Code review approved

---

## Related Documentation

- See: `temp_analysis/COMPREHENSIVE_CODE_REVIEW_v1.11.0.md`
- See: `temp_analysis/DESIGN_FLAWS_INTERACTION_MAP.md`
- See: `docs/BUG_FIXES_SUMMARY_v1.11.0.md` (will be updated with new fixes)

---

**Last Updated:** 2026-08-01  
**Status:** Ready for implementation  
**Priority:** RELEASE BLOCKER
