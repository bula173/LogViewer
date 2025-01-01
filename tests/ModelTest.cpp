#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "src/application/mvc/model.hpp"
#include "src/application/mvc/view.hpp"

class MockView : public mvc::View
{
public:
  MOCK_METHOD(void, OnDataUpdated, (), (override));
  MOCK_METHOD(void, OnCurrentIndexUpdated, (const int index), (override));
};

class ModelTest : public ::testing::Test
{
protected:
  mvc::Model<std::vector<int>> model;
  MockView mockView;

  void SetUp() override
  {
    model.RegisterOndDataUpdated(&mockView);
  }
};

TEST_F(ModelTest, NotifyDataChangedTest)
{
  EXPECT_CALL(mockView, OnDataUpdated()).Times(1);
  model.AddItem(42);
}

TEST_F(ModelTest, SetCurrentItemTest)
{
  EXPECT_CALL(mockView, OnCurrentIndexUpdated(0)).Times(1);
  model.SetCurrentItem(0);
}

TEST_F(ModelTest, AddItemTest)
{
  model.AddItem(42);
  EXPECT_EQ(model.Size(), 1);
  EXPECT_EQ(model.GetItem(0), 42);
}

TEST_F(ModelTest, GetCurrentItemIndexTest)
{
  model.SetCurrentItem(0);
  EXPECT_EQ(model.GetCurrentItemIndex(), 0);
}
