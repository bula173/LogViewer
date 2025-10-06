#include "src/application/mvc/IModel.hpp"
#include "src/application/mvc/IView.hpp"
#include <gmock/gmock.h>

namespace mvc
{
class ModelImpl : public IModel
{
  public:
    ModelImpl()
        : currentItemIndex(-1)
    {
    }

    int GetCurrentItemIndex() override
    {
        return currentItemIndex;
    }

    void SetCurrentItem(const int item) override
    {
        if (item < 0 || item >= static_cast<int>(items.size()))
        {
            throw std::out_of_range("Invalid item index");
        }
        currentItemIndex = item;
        NotifyDataChanged();
    }

    void AddItem(db::LogEvent&& item) override
    {
        items.push_back(std::move(item));
        NotifyDataChanged();
    }

    db::LogEvent& GetItem(const int index) override
    {
        if (index < 0 || index >= static_cast<int>(items.size()))
        {
            throw std::out_of_range("Invalid item index");
        }
        return items[index];
    }

    void Clear() override
    {
        items.clear();
        currentItemIndex = -1;
        NotifyDataChanged();
    }

    size_t Size() const override
    {
        return items.size();
    }

  private:
    std::vector<db::LogEvent> items;
    int currentItemIndex;
};
}

class MockView : public mvc::IView
{
  public:
    MOCK_METHOD(void, OnDataUpdated, (), (override));
    MOCK_METHOD(void, OnCurrentIndexUpdated, (const int index), (override));
};

class ModelImplTest : public ::testing::Test
{
  protected:
    mvc::ModelImpl model;
    MockView mockView;

    void SetUp() override
    {
        model.RegisterOndDataUpdated(&mockView);
    }

    void TearDown() override
    {
        // Clean up code if needed
    }
};

TEST_F(ModelImplTest, AddItem)
{
    EXPECT_CALL(mockView, OnDataUpdated()).Times(1);

    db::LogEvent event(1,
        {{"timestamp", "2025-01-14T15:20:55"}, {"type", "INFO"},
            {"info", "Test event"}});
    model.AddItem(std::move(event));

    EXPECT_EQ(model.Size(), 1);
    EXPECT_EQ(model.GetItem(0).findByKey("timestamp"), "2025-01-14T15:20:55");
}

TEST_F(ModelImplTest, SetCurrentItem)
{
    db::LogEvent event1(1,
        {{"timestamp", "2025-01-14T15:20:55"}, {"type", "INFO"},
            {"info", "Test event"}});
    db::LogEvent event2(2,
        {{"timestamp", "2025-01-15T15:20:55"}, {"type", "ERROR"},
            {"info", "Another event"}});
    model.AddItem(std::move(event1));
    model.AddItem(std::move(event2));

    EXPECT_CALL(mockView, OnDataUpdated()).Times(1);

    model.SetCurrentItem(1);

    EXPECT_EQ(model.GetCurrentItemIndex(), 1);
    EXPECT_EQ(model.GetItem(1).findByKey("timestamp"), "2025-01-15T15:20:55");
}

TEST_F(ModelImplTest, Clear)
{
    db::LogEvent event(1,
        {{"timestamp", "2025-01-14T15:20:55"}, {"type", "INFO"},
            {"info", "Test event"}});
    model.AddItem(std::move(event));

    EXPECT_CALL(mockView, OnDataUpdated()).Times(1);

    model.Clear();

    EXPECT_EQ(model.Size(), 0);
    EXPECT_EQ(model.GetCurrentItemIndex(), -1);
}

TEST_F(ModelImplTest, GetItemOutOfRange)
{
    db::LogEvent event(1,
        {{"timestamp", "2025-01-14T15:20:55"}, {"type", "INFO"},
            {"info", "Test event"}});
    model.AddItem(std::move(event));

    EXPECT_THROW(model.GetItem(1), std::out_of_range);
    EXPECT_THROW(model.GetItem(-1), std::out_of_range);
}

TEST_F(ModelImplTest, SetCurrentItemOutOfRange)
{
    EXPECT_THROW(model.SetCurrentItem(0), std::out_of_range);
    EXPECT_THROW(model.SetCurrentItem(-1), std::out_of_range);
}
