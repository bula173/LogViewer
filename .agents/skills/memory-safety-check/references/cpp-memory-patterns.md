# C++ Memory Safety Patterns & Fixes

Common memory errors in C++ and their solutions, tailored for Qt/LogViewer development.

## 1. Memory Leaks

**Symptom**: `SUMMARY: AddressSanitizer: X byte(s) leaked in Y allocation(s).`

### Pattern: Missing delete in error path

```cpp
// ❌ Leak if error() returns true
void processData() {
    MyObject* obj = new MyObject();
    if (obj->error()) {
        return;  // Oops: obj never deleted!
    }
    delete obj;
}

// ✅ Safe: RAII (Resource Acquisition Is Initialization)
void processData() {
    auto obj = std::make_unique<MyObject>();
    if (obj->error()) {
        return;  // unique_ptr cleans up automatically
    }
    // Use obj...
}
```

### Pattern: Forgotten cleanup in destructors

```cpp
// ❌ Leak in destructor
class Logger {
    char* buffer;
public:
    Logger() { buffer = new char[4096]; }
    ~Logger() { /* forgot to delete buffer */ }
};

// ✅ Safe
class Logger {
    std::unique_ptr<char[]> buffer;
public:
    Logger() : buffer(std::make_unique<char[]>(4096)) {}
    ~Logger() {}  // unique_ptr cleans up automatically
};
```

### Pattern: Callbacks holding onto deallocated objects

```cpp
// ❌ Leak and dangling pointer
class EventManager {
    std::vector<Callback> callbacks;
public:
    void subscribe(Callback cb) { callbacks.push_back(cb); }
    void broadcast() {
        for (auto& cb : callbacks) cb();  // May call deleted object!
    }
};

// ✅ Safe: Use weak_ptr for non-owning references
class EventManager {
    std::vector<std::weak_ptr<Listener>> listeners;
public:
    void subscribe(std::shared_ptr<Listener> listener) {
        listeners.push_back(listener);  // Weak reference
    }
    void broadcast() {
        for (auto it = listeners.begin(); it != listeners.end(); ) {
            if (auto listener = it->lock()) {
                listener->onEvent();
                ++it;
            } else {
                it = listeners.erase(it);  // Listener was deleted
            }
        }
    }
};
```

---

## 2. Use-After-Free (UAF)

**Symptom**: `ERROR: AddressSanitizer: heap-use-after-free`

### Pattern: Accessing after explicit delete

```cpp
// ❌ UAF
void process() {
    MyObject* obj = new MyObject();
    obj->doSomething();
    delete obj;
    obj->doMore();  // CRASH: obj is freed but we're using it!
}

// ✅ Safe
void process() {
    auto obj = std::make_unique<MyObject>();
    obj->doSomething();
    obj->doMore();
    // unique_ptr deletes at end of scope
}
```

### Pattern: Dangling pointer from container removal

```cpp
// ❌ UAF
std::vector<MyObject*> objects;
objects.push_back(new MyObject());
MyObject* ptr = objects[0];
objects.clear();  // Deletes the object
ptr->doSomething();  // CRASH: ptr is dangling!

// ✅ Safe
std::vector<std::unique_ptr<MyObject>> objects;
objects.push_back(std::make_unique<MyObject>());
// No separate ptr needed; access via objects[0] if needed
objects.clear();  // Safe cleanup
```

### Pattern: Returning reference to local variable

```cpp
// ❌ UAF (C++ dangling reference)
const MyObject& getObject() {
    MyObject local;
    return local;  // local is destroyed at function return!
}

// ✅ Safe: return by value or use heap allocation
MyObject getObject() {
    return MyObject();  // Move semantics prevent copy
}
```

---

## 3. Double-Free

**Symptom**: `ERROR: AddressSanitizer: attempting double-free`

### Pattern: Manual cleanup in destructor + explicit delete

```cpp
// ❌ Double-free
class DataHolder {
    char* data;
public:
    DataHolder() { data = new char[1024]; }
    ~DataHolder() { delete[] data; }  // First delete
    void cleanup() { delete[] data; }  // Second delete!
};

DataHolder dh;
dh.cleanup();  // Manual cleanup
// ~DataHolder called at end of scope -> CRASH!

// ✅ Safe: Use smart pointers
class DataHolder {
    std::unique_ptr<char[]> data;
public:
    DataHolder() : data(std::make_unique<char[]>(1024)) {}
    // No explicit cleanup needed or wanted
};
```

### Pattern: Copy/move semantics with raw pointers

```cpp
// ❌ Double-free if object is copied
class Cache {
    char* buffer;
public:
    Cache() { buffer = new char[4096]; }
    // No copy constructor!
};

Cache c1;
Cache c2 = c1;  // Shallow copy: c2.buffer = c1.buffer
// Both c1 and c2 delete[] buffer in destructor -> CRASH!

// ✅ Safe: Use move semantics or smart pointers
class Cache {
    std::unique_ptr<char[]> buffer;
public:
    Cache() : buffer(std::make_unique<char[]>(4096)) {}
    Cache(Cache&&) = default;  // Move constructor
    Cache(const Cache&) = delete;  // No copy
};
```

---

## 4. Buffer Overflow

**Symptom**: `ERROR: AddressSanitizer: buffer-overflow`

### Pattern: Off-by-one in array access

```cpp
// ❌ Buffer overflow
char buffer[10];
for (int i = 0; i <= 10; ++i) {  // i reaches 10, but valid indices are 0-9
    buffer[i] = 'a';  // Overflow at i=10!
}

// ✅ Safe
char buffer[10];
for (int i = 0; i < 10; ++i) {  // i goes 0-9 only
    buffer[i] = 'a';
}
```

### Pattern: String null-termination

```cpp
// ❌ No null terminator
char str[5];
strcpy(str, "hello");  // "hello" is 5 chars + '\0' = 6 bytes!

// ✅ Safe
char str[6];  // Account for null terminator
strcpy(str, "hello");

// ✅ Better: Use std::string
std::string str = "hello";
```

### Pattern: Underflow (access before buffer start)

```cpp
// ❌ Underflow
int* ptr = arr + 10;
ptr[-20] = 42;  // Accessing way before arr!

// ✅ Safe: Use container bounds checking
std::vector<int> vec(10);
if (index >= 0 && index < vec.size()) {
    vec[index] = 42;
}
```

---

## 5. Uninitialized Memory

**Symptom**: `ERROR: MemorySanitizer: use-of-uninitialized-value` (MSan)

### Pattern: Reading before writing

```cpp
// ❌ Uninitialized read
struct Config {
    int timeout;  // Not initialized in constructor
};

Config cfg;
if (cfg.timeout > 0) {  // Reading garbage!
    // ...
}

// ✅ Safe: Initialize in constructor
struct Config {
    int timeout = 30;  // Default initialization
};

// Or
struct Config {
    int timeout;
    Config() : timeout(30) {}  // Explicit initialization
};
```

### Pattern: Conditional initialization

```cpp
// ❌ Uninitialized on some paths
int result;
if (condition) {
    result = compute();
} else {
    // result is uninitialized on this path!
}
return result;

// ✅ Safe: Initialize for all paths
int result = 0;  // Default
if (condition) {
    result = compute();
}
return result;
```

### Pattern: Struct with uninitialized fields

```cpp
// ❌ Some fields uninitialized
struct Request {
    std::string url;
    int timeout;
    bool useCache;
};

Request req = {"https://example.com"};  // timeout and useCache uninitialized!

// ✅ Safe: Aggregate initialization or constructor
Request req{"https://example.com", 30, true};

// Or with designated initializers (C++20)
Request req{
    .url = "https://example.com",
    .timeout = 30,
    .useCache = true
};
```

---

## 6. Qt/QML-Specific Patterns

### Pattern: QObject parent-child relationships

```cpp
// ✅ Safe: Qt manages lifetime via parent
class MyDialog : public QDialog {
public:
    MyDialog() {
        // Ownership: this (MyDialog) owns child
        auto button = new QPushButton("OK", this);
        // button deleted when MyDialog is deleted
    }
};

// ❌ Danger: Orphaned QObject
QObject* obj = new QObject();
// No parent; calling delete obj is manual and easy to forget!
```

### Pattern: QML-C++ signals across object lifetimes

```cpp
// ❌ UAF if emitter is deleted before listener
class DataModel : public QObject {
    Q_SIGNALS:
        void dataChanged();
};

class View : public QObject {
public:
    View(DataModel* model) {
        connect(model, &DataModel::dataChanged, this, &View::refresh);
        // What if model is deleted while View still exists?
    }
    Q_SLOT void refresh() { /* */ }
};

// ✅ Safe: Use Qt's connect with context
class View : public QObject {
public:
    View(std::shared_ptr<DataModel> model) {
        connect(model.get(), &DataModel::dataChanged,
                this, &View::refresh,
                Qt::AutoConnection);  // Auto-disconnects if receiver deleted
        m_model = model;  // Keep shared_ptr alive
    }
private:
    std::shared_ptr<DataModel> m_model;
    Q_SLOT void refresh() { /* */ }
};
```

### Pattern: QVariant holding raw pointers

```cpp
// ❌ Danger: QVariant may outlive pointer
QVariant var = QVariant::fromValue((void*)myObject);
// Later: myObject deleted
void* ptr = var.value<void*>();
// ptr is dangling!

// ✅ Safe: Use QSharedPointer or store in model
class DataModel : public QAbstractListModel {
    QVector<std::shared_ptr<DataItem>> items;
};
```

---

## 7. Smart Pointer Selection

| Scenario | Use |
|----------|-----|
| Single owner, no sharing | `std::unique_ptr<T>` |
| Multiple owners needed | `std::shared_ptr<T>` |
| Observer without ownership | `std::weak_ptr<T>` |
| Array ownership | `std::unique_ptr<T[]>` |
| Qt parent-child relationship | `new MyObject(parent)` (Qt manages) |
| Circular references | `std::weak_ptr<T>` to break cycle |

---

## 8. Common Qt Memory Patterns

### QObject with owned resources

```cpp
class LogParser : public QObject {
    Q_OBJECT
public:
    LogParser(QObject* parent = nullptr) : QObject(parent) {}
    
    void parseFile(const QString& path) {
        auto file = new QFile(path);
        // BAD: orphaned file
        
        // GOOD: file as member
    }
    
private:
    std::unique_ptr<QFile> m_file;
};
```

### Signal-slot with temporary objects

```cpp
// BAD: Temporary QObject emitting signals
{
    auto obj = std::make_unique<MyObject>();
    connect(obj.get(), &MyObject::ready, this, &Receiver::onReady);
}  // obj destroyed; signal received but receiver is orphaned!

// GOOD: Keep object alive
class Manager : public QObject {
private:
    std::unique_ptr<MyObject> m_worker;
    Q_SLOT void onWorkerReady() { }
};
```

---

## 9. Testing Memory Safety

Always write tests to catch these patterns:

```cpp
TEST(MemorySafety, NoLeakOnErrorPath) {
    {
        auto obj = std::make_unique<DataLoader>();
        obj->loadInvalidFile("/nonexistent");
        // Destructor should clean up without leak
    }
    // ASan verifies no leak
}

TEST(MemorySafety, NoUseAfterFreeInCallback) {
    {
        auto mgr = std::make_unique<EventManager>();
        {
            auto listener = std::make_shared<TestListener>();
            mgr->subscribe(listener);
        }  // listener destroyed
        mgr->broadcast();  // Should not crash or use freed memory
    }
}
```

---

## Reference

- **CppCoreGuidelines**: https://github.com/isocpp/CppCoreGuidelines
- **ASan Docs**: https://github.com/google/sanitizers/wiki/AddressSanitizer
- **Qt Memory Management**: https://doc.qt.io/qt-6/objecttrees.html
