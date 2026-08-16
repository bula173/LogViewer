# LogViewer Development Roadmap

**Current Version:** 1.11.0 (Released 2026-08-15)  
**Last Updated:** 2026-08-15

---

## 📊 Current State (v1.11.0, released)

### Recent Achievements ✅
- **Customizable keyboard shortcuts** — Help → Keyboard Shortcuts is now generated from a live registry (no more stale hand-maintained list), supports rebinding/reset, and prints a cheat sheet
- **Export/Import Filter Profiles** — share `.filters.json` profiles between teammates, with QTest-driven automated coverage
- **Unified Preferences dialog** consolidating all settings
- **Local Gemma 2B inference via llama.cpp** — replaces the earlier heuristic fallback with real on-device LLM inference
- **Unified search bar** (Ctrl+F/Cmd+F) with live match counting and advanced query support
- **Dashboard tab** with statistics and report generation
- **Column width persistence, session auto-save, event tagging, keyboard navigation**
- **Weak_ptr-based MVC observer pattern** — removed the dangling-pointer risk of the old raw-pointer observer list
- **MainWindow architecture refactoring (Phase 3, in progress)** — extracting file-ops and other responsibilities into dedicated helpers

### Current Capabilities
- ✅ Multiple log format support (XML, JSON, CSV, CAN, DLT, Evlog)
- ✅ 13+ specialized analysis panels
- ✅ Advanced filtering with actor hierarchies
- ✅ Flexible dock-based UI with customization
- ✅ Local Gemma 2B AI inference (llama.cpp-backed)
- ✅ Plugin system with C-ABI stability
- ✅ Virtual list rendering (handles millions of events)

See `CHANGELOG.md` for the full, dated list of shipped changes per version.

---

## 🎯 Short-Term (Next Release)

### Medium Priority: User Experience
- **Performance Optimizations** (6-8 hours)
  - Profile large file loading (100M+ events)
  - Optimize filter reapplication
  - Reduce memory footprint for filter indices
  - Streaming progress indicators

---

## 🚀 Medium-Term (v2.0.0 - 3-6 months)

### Major Features
- **Advanced Pattern Analysis** (20+ hours)
  - Anomaly detection using statistical methods
  - Trend analysis over time windows
  - Correlation detection between fields
  - Custom pattern definitions via UI
  - ML-based pattern discovery

- **Real-Time Log Streaming** (16-20 hours)
  - Live tail mode for active log files
  - Auto-refresh with configurable interval
  - Pause/resume streaming
  - Circular buffer mode for memory-constrained environments
  - Network log streaming support

- **Multi-File Search and Analysis** (12-16 hours)
  - Search across multiple loaded log files
  - Cross-file correlation
  - Timeline view showing events from all files
  - File-aware filtering and grouping

- **Session Management** (8-10 hours)
  - Save/restore analysis sessions
  - Session history with quick-load
  - Session sharing (export as project file)
  - Auto-recovery from crashes

### Infrastructure
- **Plugin Marketplace** (12-16 hours)
  - Central repository for community plugins
  - One-click plugin installation
  - Plugin rating and reviews
  - Automatic update checking
  
- **Cloud Sync** (optional, 16-20 hours)
  - Sync settings across machines
  - Cloud-based filter/session storage
  - Collaborative session analysis
  - Status: Design phase, community interest TBD

### Performance Targets
- Handle 500M+ event files without lag
- <2 second filter reapplication
- <5 second large file loading
- Memory usage <2GB for typical workflows

---

## 💡 Long-Term Vision (v2.1+ - 6+ months)

### Advanced Analysis
- **Natural Language Queries** (TBD)
  - "Show me all errors in the past hour"
  - "Find events where CPU > 80% and Memory > 90%"
  - AI-powered query generation from natural language
  - Requires: Advanced NLP model integration

- **Distributed Log Analysis** (TBD)
  - Analyze logs from multiple servers
  - Aggregate and correlate across systems
  - Distributed timeline view
  - Requires: Network communication layer

- **Log Prediction & Forecasting** (TBD)
  - Predict next likely errors based on patterns
  - Capacity planning from resource logs
  - Trend extrapolation
  - Requires: Time-series forecasting models

### Integrations
- **External Data Sources** (TBD)
  - Import metrics from Prometheus, Grafana
  - Pull events from Elasticsearch, Splunk
  - Database query results as synthetic logs
  - Kafka streaming logs

- **Alert & Notification System** (TBD)
  - Custom alerts on filter matches
  - Slack/Teams integration
  - Email notifications
  - Webhook support

### Community Features
- **Log Sharing & Collaboration** (TBD)
  - Anonymize and share logs for debugging
  - Collaborative annotation
  - Community templates library
  - Public analysis sharing

---

## 🔧 Technical Debt & Refactoring

### High Priority
1. **EventsContainer Thread Safety** (4-6 hours)
   - Add comprehensive concurrency tests
   - Profile lock contention
   - Consider lock-free data structures for reads
   - Status: Working but stress-tested

2. **Plugin System Testing** (6-8 hours)
   - Add plugin crash isolation tests
   - Plugin API stability suite
   - Binary compatibility verification
   - Status: Functional, needs rigor

3. **Filter System Refactor** (8-10 hours)
   - Consolidate filter types
   - Performance profiling of filter pipeline
   - Lazy evaluation for expensive filters
   - Status: Works but monolithic

### Medium Priority
4. **Qt6 Upgrade** (6-8 hours)
   - Update to latest Qt 6.x
   - Test on all platforms
   - Performance comparison
   - Status: Currently on Qt 6.5+

5. **Error Handling Standardization** (6-8 hours)
   - Use Result<T,E> consistently
   - Structured error reporting
   - User-facing error messages
   - Status: Partial (new code uses Result)

6. **Logging Architecture** (4-6 hours)
   - Replace mixed logging with unified system
   - Log levels enforcement
   - Performance trace framework
   - Status: spdlog integrated, needs cleanup

### Low Priority
7. **Code Documentation** (8-10 hours)
   - API documentation (Doxygen)
   - Architecture decision records (ADRs)
   - Code examples for common tasks
   - Status: Partial

8. **Build System Optimization** (4-6 hours)
   - Reduce build time
   - Parallel compilation tweaks
   - Precompiled headers
   - Status: Currently ~2-3 minutes debug build

---

## 🐛 Known Issues & Limitations

### Current Limitations
1. **Large File Handling**
   - 500M+ events may cause slowdown
   - No streaming/chunked loading
   - Solution planned: v2.0.0 performance optimization

2. **Plugin System Limitations**
   - No GUI for plugin configuration (requires code)
   - Limited access to internal data structures
   - Binary compatibility tied to compiler version

### Design Constraints
- Single-threaded UI (Qt requirement)
- Read-lock contention during analysis
- Virtual list can't efficiently handle millions of columns
- Plugin system requires C-ABI compatibility

---

## 📈 Performance Roadmap

### Current Baselines (v1.10.0)
- Large file load (50M events): ~5 seconds
- Filter application: 100-500ms
- Sorting (10M events): 200-400ms
- Memory per 1M events: ~200MB

### Near-Term Targets
- Large file load (100M events): <10 seconds
- Filter application: 50-200ms
- Memory per 1M events: ~180MB

### v2.0.0 Targets
- Large file load (500M events): <15 seconds
- Filter application: <50ms
- Memory per 1M events: ~150MB

### v2.1+ Targets
- Distributed file load: <30 seconds for 10 remote servers
- Real-time streaming: <100ms event lag
- Memory per 1M events: <100MB

---

## 🎓 Learning & Improvements

### Architectural Improvements Made
1. **Filter Status Bar** - Real-time visualization
2. **Tab Activity Badges** - Visual feedback on updates
3. **FilterStatusBar Integration** - Observability pattern
4. **Stale Index Handling** - Defensive programming

### Lessons Learned
- Filter state management is complex with multiple update paths
- Qt signal/slot patterns require careful ordering
- Config persistence needs validation on load
- Thread safety requires defensive checks at boundaries

### Best Practices Established
1. Add defensive checks before accessing cached data
2. Validate indices before container access
3. Log state transitions for debugging
4. Test merge/sort interaction paths
5. Use Result<T,E> for fallible operations

---

## 📋 Release Checklist Template

### For Each Release
- [ ] Update version in CMakeLists.txt
- [ ] Update CHANGELOG.md with all changes
- [ ] Run full test suite (ctest)
- [ ] Performance profile on large files
- [ ] Test on all supported platforms (macOS, Windows, Linux)
- [ ] Update documentation
- [ ] Create GitHub release with notes
- [ ] Update website/download page
- [ ] Announce on community channels

### Regression Testing
- [ ] Merge + sort interaction
- [ ] Filter application with active filters
- [ ] Column reordering and persistence
- [ ] Large file loading
- [ ] Plugin system stability
- [ ] All UI panels responsive

---

## 🤝 Community & Contribution

### How to Contribute
1. Pick an issue from the roadmap
2. Discuss design in GitHub discussions
3. Submit PR with tests
4. Code review process
5. Merge to main

### Plugin Development Opportunities
- Custom log parsers for proprietary formats
- Specialized analysis panels
- Export format handlers
- Cloud storage integrations
- Monitoring service integrations

### Documentation Needs
- Video tutorials for common workflows
- Plugin development guide
- Filter syntax documentation
- Performance tuning guide

---

## 🔮 Future Vision (Speculative)

### Possible Directions (Not Committed)
- **GPU Acceleration** for large file processing
- **Web UI** for remote log analysis
- **Mobile App** for log browsing on the go
- **AI-Powered Debugging Assistant** beyond pattern matching
- **Federated Search** across multiple log sources
- **Real-time Collaboration** on log analysis

### Community Requests (Placeholder)
- [ ] Add your feature request via GitHub Issues
- [ ] Vote on existing feature requests
- [ ] Join community discussions
- [ ] Contribute to plugin ecosystem

---

## 📞 Questions & Contact

- **GitHub Issues**: Report bugs and request features
- **GitHub Discussions**: Ask questions and discuss ideas
- **Contributing**: See CONTRIBUTING.md
- **Email**: Development team contact (TBD)

---

## Version History

| Version | Date | Key Changes |
|---------|------|-------------|
| 1.11.0 | 2026-08-15 | Dashboard tab, unified search bar, weak_ptr observer pattern, SearchEngine/build-system/CI fixes |
| 1.10.0 | 2026-07-31 | Unified Preferences, real Gemma/llama.cpp inference, theme customization, notifications, report generation |
| 1.7.2 | 2026-06-02 | Actor discovery, sequence diagrams, startup update check |
| 1.7.1 | 2026-06-02 | Consolidated plugin management |
| 1.7.0 | 2026-06-01 | Named layouts, side-by-side comparison, file tailing, JSON/NDJSON parser |
| 1.6.0 | 2026-05-22 | Previous stable release |

See `CHANGELOG.md` for full details per version.

---

**Last Reviewed:** 2026-08-14  
**Next Review:** 2026-09-14  
**Maintained By:** LogViewer Development Team

This roadmap is a living document and subject to change based on community feedback, technical constraints, and priority shifts.
