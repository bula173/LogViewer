# Architecture Analysis & Improvement Proposals

## Executive Summary

LogViewer's architecture is well-structured with clean separation of concerns, but the **plugin system** and **MVC observer pattern** can be significantly improved using modern C++20 features. This document proposes targeted improvements that maintain backward compatibility while enhancing safety, type-safety, and maintainability.

**Key Findings:**
- ✅ Clean layered architecture (Presentation, Business Logic, Data)
- ⚠️ Plugin system uses unsafe void* function pointers
- ⚠️ MVC observer pattern uses raw pointers without lifetime management
- ⚠️ No plugin dependency resolution or safe initialization order
- ✅ Thread-safe data layer with shared_mutex

---

## Part 1: Plugin System Issues

### Issue 1.1: Type-Unsafe Function Pointer Management

**Current Approach** (`PluginLoadInfo` struct):
```cpp
struct PluginLoadInfo {
    void* pluginCreateLeftPanel = nullptr;
    void* pluginCreateRightPanel = nullptr;
    void* pluginCreateBottomPanel = nullptr;
    void* pluginCreateMainPanel = nullptr;
    void* pluginDestroyPanelWidget = nullptr;
    void* pluginCreateAIService = nullptr;
    void* pluginDestroyAIService = nullptr;
    void* pluginSetEventsCallbacks = nullptr;
    void* pluginSetHostUiCallbacks = nullptr;
};
```

**Problems:**
- No compile-time signature verification
- Runtime casting errors undetected
- DLL boundary issues if signatures mismatch
- Multiple identical casting code throughout PluginManager

**Solution:** Template-Based Type-Safe Registry

```cpp
// PluginFunctionRegistry.hpp
template<typename FnType>
class FunctionRegistry {
public:
    FunctionRegistry(void* handle, const std::string& symbolName)
        : m_handle(handle), m_symbolName(symbolName) {
        m_function = ResolveSymbol<FnType>();
    }

    explicit operator bool() const { return m_function != nullptr; }

    template<typename... Args>
    requires std::invocable_v<FnType, Args...>
    auto call(Args&&... args) const {
        if (!m_function) {
            throw std::runtime_error("Function not loaded: " + m_symbolName);
        }
        return m_function(std::forward<Args>(args)...);
    }

private:
    template<typename Fn>
    Fn ResolveSymbol() const {
        #ifdef _WIN32
            auto ptr = GetProcAddress(static_cast<HMODULE>(m_handle),
                                     m_symbolName.c_str());
        #else
            auto ptr = dlsym(m_handle, m_symbolName.c_str());
        #endif
        return ptr ? reinterpret_cast<Fn>(ptr) : nullptr;
    }
};

// Usage
if (auto creator = info.panelCreators[PanelType::Left]) {
    auto* widget = creator.call(handle, parent, settings);
}
```

**Benefits:**
- ✅ Compile-time type safety via concepts
- ✅ Automatic error handling
- ✅ No manual reinterpret_cast
- ✅ Clear intent and error messages

### Issue 1.2: Duplicate Panel Creation Functions

**Current Approach:**
```cpp
info.pluginCreateLeftPanel = probe_sym("Plugin_CreateLeftPanel");
info.pluginCreateRightPanel = probe_sym("Plugin_CreateRightPanel");
info.pluginCreateBottomPanel = probe_sym("Plugin_CreateBottomPanel");
info.pluginCreateMainPanel = probe_sym("Plugin_CreateMainPanel");
```

**Problems:**
- No extensibility for new panel types
- Repeated probe/cast logic
- Hard to support custom panel locations

**Solution:** Extensible Panel Type System

```cpp
enum class PanelType { Left, Right, Bottom, Main, Custom };

struct PluginLoadInfo {
    std::map<PanelType, FunctionRegistry<CreatePanelFn>> panelCreators;
    std::optional<FunctionRegistry<DestroyPanelFn>> panelDestroyer;
};

// Plugin discovery becomes data-driven
const std::vector<std::pair<PanelType, std::string>> PANEL_SYMBOLS = {
    {PanelType::Left, "Plugin_CreateLeftPanel"},
    {PanelType::Right, "Plugin_CreateRightPanel"},
    {PanelType::Bottom, "Plugin_CreateBottomPanel"},
    {PanelType::Main, "Plugin_CreateMainPanel"},
};

// Automatic discovery
for (auto& [type, symbol] : PANEL_SYMBOLS) {
    if (auto reg = FunctionRegistry<CreatePanelFn>(handle, symbol)) {
        info.panelCreators[type] = std::move(reg);
    }
}
```

**Benefits:**
- ✅ Centralized panel type definition
- ✅ Easy to add new panel types
- ✅ Cleaner PluginManager code
- ✅ Less duplication

### Issue 1.3: Mixed C++ and C-ABI Abstractions

**Current State:**
- IPlugin.hpp: C++ virtual interfaces
- PluginManager.cpp: C-ABI function pointer calls
- Two different extension mechanisms

**Solution:** Consolidate Around IPlugin

```cpp
// Extend IPlugin interface with optional methods
class IPlugin {
    // Existing methods
    virtual bool Initialize() = 0;
    virtual void Shutdown() = 0;

    // New optional panel methods
    virtual std::optional<std::unique_ptr<QWidget>>
    CreatePanelWidget(PanelType type, QWidget* parent) {
        return std::nullopt;  // Optional
    }

    // Framework provides default C-ABI compatibility wrapper
};

// PluginManager can try C++ interface first, fall back to C-ABI
std::unique_ptr<QWidget> CreatePanel(IPlugin* plugin, PanelType type, QWidget* parent) {
    // Try C++ method first (safer)
    if (auto widget = plugin->CreatePanelWidget(type, parent)) {
        return std::move(*widget);
    }

    // Fall back to C-ABI for legacy plugins
    if (auto* creator = GetCABIPanelCreator(type)) {
        return std::unique_ptr<QWidget>(creator(...));
    }

    return nullptr;
}
```

**Benefits:**
- ✅ Single abstraction for new plugins
- ✅ Backward compatible with C-ABI
- ✅ Cleaner API for developers
- ✅ Type-safe by default

---

## Part 2: MVC Observer Pattern Issues

### Issue 2.1: Raw Pointer Observer Pattern

**Current Code** (`IModel.hpp`):
```cpp
class IModel {
  protected:
    std::vector<IView*> m_views;  // Raw pointers - dangling risk!

    virtual void RegisterOndDataUpdated(IView* view) {
        m_views.push_back(view);
    }

    virtual void NotifyDataChanged() {
        for (auto v : m_views) {
            v->OnDataUpdated();  // Could be dangling pointer!
        }
    }
};
```

**Dangling Pointer Scenario:**
```cpp
auto container = std::make_unique<EventsContainer>();
auto view = std::make_unique<ItemDetailsView>();

container->RegisterOndDataUpdated(view.get());
view.reset();  // View destroyed!

container->NotifyDataChanged();  // CRASH - dangling pointer access
```

**Solution: Weak Pointer Pattern**

```cpp
// IModel.hpp - New safe interface
class IModelObservable {
public:
    using ViewPtr = std::shared_ptr<IView>;

    void RegisterView(ViewPtr view) {
        std::unique_lock lock(m_viewsMutex);
        m_weakViews.push_back(std::weak_ptr<IView>(view));
        CleanupDeadViews();
    }

    void UnregisterView(const IView* view) {
        std::unique_lock lock(m_viewsMutex);
        m_weakViews.erase(
            std::remove_if(m_weakViews.begin(), m_weakViews.end(),
                [view](const auto& weak) {
                    auto shared = weak.lock();
                    return !shared || shared.get() == view;
                }),
            m_weakViews.end()
        );
    }

protected:
    void NotifyDataChanged() {
        std::shared_lock lock(m_viewsMutex);

        for (auto& weak : m_weakViews) {
            if (auto view = weak.lock()) {
                view->OnDataUpdated();  // Safe!
            }
        }
    }

private:
    mutable std::shared_mutex m_viewsMutex;
    std::vector<std::weak_ptr<IView>> m_weakViews;

    void CleanupDeadViews() {
        m_weakViews.erase(
            std::remove_if(m_weakViews.begin(), m_weakViews.end(),
                [](const auto& weak) { return weak.expired(); }),
            m_weakViews.end()
        );
    }
};

// For backward compatibility
class IModel : public IModelObservable {
public:
    [[deprecated("Use RegisterView(shared_ptr) instead")]]
    virtual void RegisterOndDataUpdated(IView* view);
};
```

**Safe Usage:**
```cpp
auto container = std::make_shared<EventsContainer>();
auto view = std::make_shared<ItemDetailsView>();

container->RegisterView(view);  // Uses weak_ptr internally
view.reset();  // View destroyed, weak_ptr detects this

container->NotifyDataChanged();  // Safe: weak_ptr returns nullptr
```

**Benefits:**
- ✅ Automatic cleanup of destroyed views
- ✅ No dangling pointers
- ✅ Thread-safe with shared_mutex
- ✅ Zero-overhead (compared to raw pointer + null checks)

### Issue 2.2: No Observer Lifecycle Events

**Current Problem:**
Views can't react to being attached/detached from model

**Solution: Lifecycle Hooks**

```cpp
class IView {
    virtual void OnDataUpdated() = 0;
    virtual void OnCurrentIndexUpdated(int index) = 0;

    // NEW: Lifecycle notifications
    virtual void OnAttachedToModel(IModelObservable* model) {}
    virtual void OnDetachedFromModel() {}
};

class IModelObservable {
    void RegisterView(ViewPtr view) {
        std::unique_lock lock(m_viewsMutex);
        m_weakViews.push_back(std::weak_ptr<IView>(view));

        // Notify view of attachment
        view->OnAttachedToModel(this);
    }

    void UnregisterView(const IView* view) {
        std::unique_lock lock(m_viewsMutex);

        // Find and notify before removing
        for (auto& weak : m_weakViews) {
            if (auto v = weak.lock(); v && v.get() == view) {
                v->OnDetachedFromModel();
                break;
            }
        }

        // Remove from list
        m_weakViews.erase(...);
    }
};
```

**Benefits:**
- ✅ Views can initialize/cleanup based on model
- ✅ Better separation of concerns
- ✅ Testability improvements

---

## Part 3: Plugin Dependency Management

### Issue 3.1: No Dependency Resolution

**Problem:**
- Plugins loaded in arbitrary order
- Circular dependencies not detected
- Failed initialization due to missing dependencies

**Solution: Dependency Graph**

```cpp
class PluginDependencyGraph {
public:
    struct Node {
        std::string pluginId;
        std::vector<std::string> dependencies;
    };

    // Validate no circular dependencies
    util::Result<bool, error::Error> ValidateNoCycles() const {
        std::set<std::string> visited, recursionStack;

        for (const auto& [id, _] : m_nodes) {
            if (HasCycle(id, visited, recursionStack)) {
                return util::Result<bool, error::Error>::Err(
                    error::Error("Circular dependency at: " + id));
            }
        }
        return util::Result<bool, error::Error>::Ok(true);
    }

    // Get topological sort for initialization order
    util::Result<std::vector<std::string>, error::Error>
    GetInitializationOrder() const {
        if (auto val = ValidateNoCycles(); val.isErr()) {
            return util::Result<std::vector<std::string>, error::Error>::Err(val.error());
        }

        std::vector<std::string> order;
        std::set<std::string> visited;

        for (const auto& [id, _] : m_nodes) {
            TopologicalSort(id, visited, order);
        }

        std::reverse(order.begin(), order.end());
        return util::Result<std::vector<std::string>, error::Error>::Ok(order);
    }

    // Get reverse order for shutdown
    util::Result<std::vector<std::string>, error::Error>
    GetShutdownOrder() const {
        if (auto order = GetInitializationOrder(); order.isOk()) {
            auto shutdown = order.unwrap();
            std::reverse(shutdown.begin(), shutdown.end());
            return util::Result<std::vector<std::string>, error::Error>::Ok(shutdown);
        }
        return order;
    }
};
```

**Usage:**
```cpp
PluginDependencyGraph graph;
for (const auto& [id, info] : m_plugins) {
    graph.AddNode(id, info.instance->GetMetadata().dependencies);
}

// Validate and get initialization order
if (auto order = graph.GetInitializationOrder(); order.isOk()) {
    for (const auto& id : order.unwrap()) {
        EnablePlugin(id);  // Safe: dependencies already initialized
    }
} else {
    util::Logger::Error("Circular dependency detected");
}
```

**Benefits:**
- ✅ Prevents circular dependency issues
- ✅ Guarantees safe initialization order
- ✅ Detects missing dependencies early
- ✅ Improves reliability

---

## Part 4: Plugin Event System

### Enhancement: Fine-Grained Events

**Current:**
```cpp
enum class PluginEvent {
    Loaded, Unloaded, Enabled, Disabled, Errored
};
```

**Proposed Enhancement:**
```cpp
enum class PluginEventType {
    // Lifecycle
    Loaded, Unloaded,
    Enabled, Disabled,

    // Initialization
    InitializationStarted,
    InitializationCompleted,
    InitializationFailed,

    // Configuration
    ConfigurationChanged,

    // Errors
    ErrorOccurred,

    // Resources
    PanelCreated,
    PanelDestroyed,
};

struct PluginEvent {
    PluginEventType type;
    std::string pluginId;
    IPlugin* plugin;
    std::string details;
    std::chrono::system_clock::time_point timestamp;
};

// Event bus with automatic observer cleanup
class PluginEventBus {
    void Subscribe(std::shared_ptr<IPluginEventObserver> observer) {
        std::unique_lock lock(m_mutex);
        m_observers.push_back(observer);  // weak_ptr internally
    }

    void PublishEvent(const PluginEvent& event) {
        std::shared_lock lock(m_mutex);

        for (auto& weak : m_observers) {
            if (auto obs = weak.lock()) {
                try {
                    obs->OnPluginEvent(event);
                } catch (const std::exception& e) {
                    util::Logger::Error("Observer error: {}", e.what());
                }
            }
        }
    }
};
```

**Benefits:**
- ✅ Fine-grained observability
- ✅ Better debugging and logging
- ✅ Automatic observer cleanup
- ✅ Error resilience

---

## Implementation Priority & Roadmap

### Phase 1: Core Safety (v1.1.0)
**Priority:** Critical
**Timeline:** 1-2 weeks
**Items:**
1. Implement weak_ptr observer pattern for MVC
2. Add deprecation warnings to raw pointer API
3. Migrate tests to use shared_ptr

**Impact:** Eliminates dangling pointer bugs
**Risk:** LOW (new interfaces alongside existing ones)

### Phase 2: Plugin System (v1.1.0)
**Priority:** High
**Timeline:** 2 weeks
**Items:**
1. Implement type-safe function registry
2. Add dependency graph validation
3. Implement safe initialization/shutdown order

**Impact:** Prevents plugin loading crashes
**Risk:** LOW (backward compatible C-ABI)

### Phase 3: Enhanced Events & Extensibility (v1.2.0)
**Priority:** Medium
**Timeline:** 1-2 weeks
**Items:**
1. Enhance plugin event system
2. Add lifecycle hooks to observers
3. Create unified plugin panel interface

**Impact:** Better plugin developer experience
**Risk:** VERY LOW (Optional new features)

### Phase 4: Deprecation Removal (v2.0.0)
**Priority:** Low
**Timeline:** Future
**Items:**
1. Remove deprecated raw pointer observer API
2. Consolidate plugin interfaces
3. Remove C-ABI fallback for new APIs

**Impact:** Cleaner codebase
**Risk:** MEDIUM (Breaking change for major version)

---

## Risk Assessment & Mitigation

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|-----------|
| Plugin ABI break | LOW | MEDIUM | Add plugin API version, maintain backward compatibility layer |
| Observer migration issues | LOW | LOW | Deprecation warnings, migration guide, examples |
| Performance regression | VERY LOW | LOW | Profile with weak_ptr (minimal overhead), benchmark before/after |
| Test failures | MEDIUM | LOW | Add comprehensive unit tests for new patterns |

**Backward Compatibility Strategy:**
- Keep old IModel::RegisterOndDataUpdated(IView*) method
- Mark as `[[deprecated]]` with replacement suggestion
- Support both raw pointers and shared_ptr simultaneously
- Maintain C-ABI compatibility for plugins
- Version plugin API (currently 1.0.0)

---

## Success Criteria

### Code Quality
- ✅ Zero dangling pointer bugs
- ✅ All observers use weak_ptr
- ✅ All function pointers wrapped in type-safe registry
- ✅ 95%+ test coverage for new code

### Performance
- ✅ <1% overhead from weak_ptr (shared_ptr already used elsewhere)
- ✅ No performance regression in hot paths
- ✅ Build time unchanged

### Documentation
- ✅ Migration guide for plugin developers
- ✅ Updated API examples
- ✅ Architecture diagrams updated
- ✅ Doxygen comments for all new APIs

### Developer Experience
- ✅ New plugins easier to write (type-safe by default)
- ✅ Dependencies resolved automatically
- ✅ Clearer error messages
- ✅ Better IDE support (type information)

---

## Conclusion

The proposed improvements modernize LogViewer's architecture using C++20 features while maintaining full backward compatibility. The phased approach allows gradual adoption and minimal disruption.

**Key Benefits:**
- 🔒 **Safety:** Eliminates dangling pointer risks
- 🎯 **Type Safety:** Replaces unsafe void* with templates
- 📦 **Maintainability:** Reduces complexity and code duplication
- 🧩 **Extensibility:** Easier for plugin developers
- ⚡ **Performance:** Zero-overhead abstractions

**Recommendation:** Implement Phase 1 and 2 in v1.1.0 (planned for Q2 2026) to capture most safety benefits while Phase 3-4 can follow in subsequent releases.

