/**
 * @file ArchitectureImprovementsTests.cpp
 * @brief Unit tests for architectural improvements
 * @note These are template tests demonstrating the new patterns
 * @author LogViewer Development Team
 */

#include <gtest/gtest.h>
#include "PluginFunctionRegistry.hpp"
#include "PluginDependencyGraph.hpp"
#include "IModelObservable.hpp"
#include "PluginEventBus.hpp"
#include <memory>
#include <dlfcn.h>

// ============================================================================
// Test: Type-Safe Plugin Function Registry
// ============================================================================

TEST(PluginFunctionRegistryTest, ResolvesValidSymbol)
{
    // Test symbol resolution (example with libc)
    void* handle = dlopen(nullptr, RTLD_LAZY);
    ASSERT_NE(handle, nullptr);

    // Use strlen as test function - it's guaranteed to exist
    using StrlenFn = size_t(*)(const char*);
    plugin::FunctionRegistry<StrlenFn> strlen_fn(handle, "strlen");

    EXPECT_TRUE(strlen_fn);  // Should resolve
    if (strlen_fn) {
        EXPECT_EQ(strlen_fn.call("hello"), 5);
    }

    dlclose(handle);
}

TEST(PluginFunctionRegistryTest, FailsInvalidSymbol)
{
    void* handle = dlopen(nullptr, RTLD_LAZY);
    ASSERT_NE(handle, nullptr);

    using DummyFn = void(*)();
    plugin::FunctionRegistry<DummyFn> dummy_fn(handle, "NonExistentSymbolXYZ");

    EXPECT_FALSE(dummy_fn);  // Should not resolve

    EXPECT_THROW(dummy_fn.call(), std::runtime_error);

    dlclose(handle);
}

TEST(PluginFunctionRegistryTest, CallsResolvedFunction)
{
    void* handle = dlopen(nullptr, RTLD_LAZY);
    ASSERT_NE(handle, nullptr);

    using PutcharFn = int(*)(int);
    plugin::FunctionRegistry<PutcharFn> putchar_fn(handle, "putchar");

    if (putchar_fn) {
        // Call resolved function - no exception
        int result = putchar_fn.call('X');
        EXPECT_GT(result, -1);  // putchar returns character or EOF
    }

    dlclose(handle);
}

// ============================================================================
// Test: Plugin Dependency Graph
// ============================================================================

TEST(PluginDependencyGraphTest, ValidatesNoCycles)
{
    plugin::PluginDependencyGraph graph;
    graph.AddNode("A", {});
    graph.AddNode("B", {"A"});
    graph.AddNode("C", {"B"});

    auto result = graph.ValidateNoCycles();
    EXPECT_TRUE(result.isOk());
}

TEST(PluginDependencyGraphTest, DetectsCycles)
{
    plugin::PluginDependencyGraph graph;
    graph.AddNode("A", {"B"});
    graph.AddNode("B", {"C"});
    graph.AddNode("C", {"A"});  // Circular!

    auto result = graph.ValidateNoCycles();
    EXPECT_TRUE(result.isErr());
}

TEST(PluginDependencyGraphTest, ComputesInitializationOrder)
{
    plugin::PluginDependencyGraph graph;
    graph.AddNode("C", {"B"});
    graph.AddNode("B", {"A"});
    graph.AddNode("A", {});

    auto result = graph.GetInitializationOrder();
    ASSERT_TRUE(result.isOk());

    auto order = result.unwrap();
    EXPECT_EQ(order.size(), 3);
    EXPECT_EQ(order[0], "A");  // A initialized first
    EXPECT_EQ(order[1], "B");  // Then B
    EXPECT_EQ(order[2], "C");  // Then C
}

TEST(PluginDependencyGraphTest, ComputesShutdownOrderReversed)
{
    plugin::PluginDependencyGraph graph;
    graph.AddNode("A", {});
    graph.AddNode("B", {"A"});
    graph.AddNode("C", {"B"});

    auto shutdown = graph.GetShutdownOrder();
    ASSERT_TRUE(shutdown.isOk());

    auto order = shutdown.unwrap();
    EXPECT_EQ(order[0], "C");  // C shutdown first
    EXPECT_EQ(order[1], "B");
    EXPECT_EQ(order[2], "A");  // A shutdown last
}

TEST(PluginDependencyGraphTest, ValidatesMissingDependencies)
{
    plugin::PluginDependencyGraph graph;
    graph.AddNode("A", {"NonExistent"});

    std::set<std::string> available = {"A"};
    auto result = graph.ValidateDependenciesAvailable(available);

    EXPECT_TRUE(result.isErr());
}

// ============================================================================
// Test: MVC Observer Pattern with Weak Pointers
// ============================================================================

// Mock view for testing
class MockView : public mvc::IView {
  public:
    void OnDataUpdated() override { dataUpdatedCount++; }
    void OnCurrentIndexUpdated(int index) override {
        currentIndexUpdatedCount++;
        lastIndex = index;
    }
    void OnAttachedToModel(mvc::IModelObservable* model) override {
        attachedCount++;
    }
    void OnDetachedFromModel() override { detachedCount++; }

    int dataUpdatedCount = 0;
    int currentIndexUpdatedCount = 0;
    int attachedCount = 0;
    int detachedCount = 0;
    int lastIndex = -1;
};

// Mock model for testing
class MockModel : public mvc::IModelObservable {
  public:
    void TriggerDataChanged() { NotifyDataChanged(); }
    void TriggerIndexChanged(int index) { NotifyCurrentIndexUpdated(index); }
};

TEST(ModelObservableTest, RegistersAndNotifiesViews)
{
    auto model = std::make_shared<MockModel>();
    auto view = std::make_shared<MockView>();

    model->RegisterView(view);
    EXPECT_EQ(model->GetViewCount(), 1);

    model->TriggerDataChanged();
    EXPECT_EQ(view->dataUpdatedCount, 1);
}

TEST(ModelObservableTest, RemovesDestroyedViews)
{
    auto model = std::make_shared<MockModel>();
    auto view = std::make_shared<MockView>();

    model->RegisterView(view);
    EXPECT_EQ(model->GetViewCount(), 1);

    view.reset();  // Destroy view
    EXPECT_EQ(view.use_count(), 0);

    model->TriggerDataChanged();  // Should not crash
    EXPECT_EQ(model->GetViewCount(), 0);  // View auto-removed
}

TEST(ModelObservableTest, HandlesMultipleViews)
{
    auto model = std::make_shared<MockModel>();
    auto view1 = std::make_shared<MockView>();
    auto view2 = std::make_shared<MockView>();

    model->RegisterView(view1);
    model->RegisterView(view2);
    EXPECT_EQ(model->GetViewCount(), 2);

    model->TriggerDataChanged();
    EXPECT_EQ(view1->dataUpdatedCount, 1);
    EXPECT_EQ(view2->dataUpdatedCount, 1);

    view1.reset();  // Destroy one view
    EXPECT_EQ(model->GetViewCount(), 1);

    model->TriggerDataChanged();
    EXPECT_EQ(view2->dataUpdatedCount, 2);
}

TEST(ModelObservableTest, NotifiesIndexUpdates)
{
    auto model = std::make_shared<MockModel>();
    auto view = std::make_shared<MockView>();

    model->RegisterView(view);
    model->TriggerIndexChanged(42);

    EXPECT_EQ(view->currentIndexUpdatedCount, 1);
    EXPECT_EQ(view->lastIndex, 42);
}

TEST(ModelObservableTest, CallsLifecycleHooks)
{
    auto model = std::make_shared<MockModel>();
    auto view = std::make_shared<MockView>();

    EXPECT_EQ(view->attachedCount, 0);

    model->RegisterView(view);
    EXPECT_EQ(view->attachedCount, 1);

    model->UnregisterView(view.get());
    EXPECT_EQ(view->detachedCount, 1);
}

// ============================================================================
// Test: Plugin Event Bus
// ============================================================================

class MockPluginEventObserver : public plugin::IPluginEventObserver {
  public:
    void OnPluginEvent(const plugin::PluginEvent& event) override {
        receivedEvents.push_back(event.type);
    }

    std::vector<plugin::PluginEventType> receivedEvents;
};

TEST(PluginEventBusTest, SubscribesAndPublishesEvents)
{
    auto& bus = plugin::PluginEventBus::GetInstance();
    auto observer = std::make_shared<MockPluginEventObserver>();

    bus.Subscribe(observer);

    plugin::PluginEvent event{plugin::PluginEventType::Loaded, "test-plugin",
                             nullptr, ""};
    bus.PublishEvent(event);

    EXPECT_EQ(observer->receivedEvents.size(), 1);
    EXPECT_EQ(observer->receivedEvents[0], plugin::PluginEventType::Loaded);
}

TEST(PluginEventBusTest, RemovesDestroyedObservers)
{
    auto& bus = plugin::PluginEventBus::GetInstance();

    {
        auto observer = std::make_shared<MockPluginEventObserver>();
        bus.Subscribe(observer);
        // Observer destroyed here
    }

    // Bus should have 0 observers after cleanup
    plugin::PluginEvent event{plugin::PluginEventType::Loaded, "test", nullptr, ""};
    bus.PublishEvent(event);
    // Should not crash
}

TEST(PluginEventBusTest, NotifiesMultipleObservers)
{
    auto& bus = plugin::PluginEventBus::GetInstance();
    auto obs1 = std::make_shared<MockPluginEventObserver>();
    auto obs2 = std::make_shared<MockPluginEventObserver>();

    bus.Subscribe(obs1);
    bus.Subscribe(obs2);

    plugin::PluginEvent event{plugin::PluginEventType::InitializationCompleted,
                             "test", nullptr, ""};
    bus.PublishEvent(event);

    EXPECT_EQ(obs1->receivedEvents.size(), 1);
    EXPECT_EQ(obs2->receivedEvents.size(), 1);
}
