#include <gtest/gtest.h>
#include <casket/types/intrusive_list.hpp>

using namespace casket;

struct ListNode : IntrusiveListNode<ListNode>
{
    int value;

    explicit ListNode(int v)
        : value(v)
    {
    }
};

class IntrusiveListTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        node1 = new ListNode(1);
        node2 = new ListNode(2);
        node3 = new ListNode(3);
    }

    void TearDown() override
    {
        delete node1;
        delete node2;
        delete node3;
    }

    IntrusiveList<ListNode> list;
    ListNode* node1;
    ListNode* node2;
    ListNode* node3;
};

TEST_F(IntrusiveListTest, IsInitiallyEmpty)
{
    ASSERT_TRUE(list.empty());
    ASSERT_EQ(list.size(), 0);
}

TEST_F(IntrusiveListTest, PushFrontWorks)
{
    list.push_front(*node1);
    ASSERT_EQ(list.front().value, 1);
    ASSERT_EQ(list.back().value, 1);
    ASSERT_EQ(list.size(), 1);

    list.push_front(*node2);
    ASSERT_EQ(list.front().value, 2);
    ASSERT_EQ(list.back().value, 1);
    ASSERT_EQ(list.size(), 2);
}

TEST_F(IntrusiveListTest, PushBackWorks)
{
    list.push_back(*node1);
    ASSERT_EQ(list.front().value, 1);
    ASSERT_EQ(list.back().value, 1);

    list.push_back(*node2);
    ASSERT_EQ(list.front().value, 1);
    ASSERT_EQ(list.back().value, 2);
}

TEST_F(IntrusiveListTest, IterationWorks)
{
    list.push_back(*node1);
    list.push_back(*node2);
    list.push_back(*node3);

    int sum = 0;
    for (const auto& node : list)
    {
        sum += node.value;
    }
    ASSERT_EQ(sum, 6);

    auto it = list.begin();
    ASSERT_EQ(it->value, 1);
    ++it;
    ASSERT_EQ(it->value, 2);
    --it;
    ASSERT_EQ(it->value, 1);
}

TEST_F(IntrusiveListTest, RemoveFromMiddle)
{
    list.push_back(*node1);
    list.push_back(*node2);
    list.push_back(*node3);

    list.remove(*node2);
    ASSERT_EQ(list.size(), 2);
    ASSERT_EQ(list.front().value, 1);
    ASSERT_EQ(list.back().value, 3);
    ASSERT_EQ(node2->next, nullptr);
    ASSERT_EQ(node2->prev, nullptr);
}

TEST_F(IntrusiveListTest, RemoveFirst)
{
    list.push_back(*node1);
    list.push_back(*node2);

    list.remove(*node1);
    ASSERT_EQ(list.size(), 1);
    ASSERT_EQ(list.front().value, 2);
}

TEST_F(IntrusiveListTest, RemoveLast)
{
    list.push_back(*node1);
    list.push_back(*node2);

    list.remove(*node2);
    ASSERT_EQ(list.size(), 1);
    ASSERT_EQ(list.back().value, 1);
}

TEST_F(IntrusiveListTest, ClearWorks)
{
    list.push_back(*node1);
    list.push_back(*node2);
    list.clear();

    ASSERT_TRUE(list.empty());
    ASSERT_EQ(node1->next, nullptr);
    ASSERT_EQ(node1->prev, nullptr);
    ASSERT_EQ(node2->next, nullptr);
    ASSERT_EQ(node2->prev, nullptr);
}

TEST_F(IntrusiveListTest, RemoveNonExistingDoesNothing)
{
    list.remove(*node1);
    ASSERT_TRUE(list.empty());
}

TEST_F(IntrusiveListTest, SelfReferenceWorks)
{
    list.push_front(*node1);
    list.remove(*node1);
    list.push_front(*node1);
    ASSERT_EQ(list.size(), 1);
}

TEST_F(IntrusiveListTest, MultipleListsIndependent)
{
    IntrusiveList<ListNode> list2;
    ListNode node4(4), node5(5);

    list.push_back(*node1);
    list.push_back(*node2);
    list2.push_back(*node3);
    list2.push_back(node4);
    list2.push_back(node5);

    ASSERT_EQ(list.size(), 2);
    ASSERT_EQ(list2.size(), 3);

    list.remove(*node1);
    ASSERT_EQ(list.size(), 1);
    ASSERT_EQ(list2.size(), 3);
}

TEST_F(IntrusiveListTest, ConstMethodsWork)
{
    list.push_back(*node1);
    list.push_back(*node2);

    const auto& const_list = list;
    ASSERT_EQ(const_list.front().value, 1);
    ASSERT_EQ(const_list.back().value, 2);
    ASSERT_EQ(const_list.size(), 2);
    ASSERT_FALSE(const_list.empty());
}

