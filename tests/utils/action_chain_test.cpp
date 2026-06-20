#include <gtest/gtest.h>
#include <casket/utils/action_chain.hpp>

using namespace casket;

TEST(ActionChainTest, EmptyChain)
{
    ActionChain chain;
    EXPECT_EQ(chain.size(), 0);
    EXPECT_FALSE(chain.isCommitted());

    chain.execute();
    EXPECT_TRUE(chain.isCommitted());
}

TEST(ActionChainTest, SingleActionSuccess)
{
    ActionChain chain;
    int value = 0;

    chain.addAction(
        [&]()
        {
            value = 42;
        },
        [&]()
        {
            value = 0;
        });

    EXPECT_EQ(chain.size(), 1);
    chain.execute();
    EXPECT_EQ(value, 42);
    EXPECT_TRUE(chain.isCommitted());
}

TEST(ActionChainTest, MultipleActionsSuccess)
{
    ActionChain chain;
    int counter = 0;

    chain.addAction(
        [&]()
        {
            counter += 10;
        },
        [&]()
        {
            counter -= 10;
        });
    chain.addAction(
        [&]()
        {
            counter += 5;
        },
        [&]()
        {
            counter -= 5;
        });
    chain.addAction(
        [&]()
        {
            counter += 1;
        },
        [&]()
        {
            counter -= 1;
        });

    chain.execute();
    EXPECT_EQ(counter, 16);
    EXPECT_TRUE(chain.isCommitted());
}

TEST(ActionChainTest, ActionWithoutRollback)
{
    ActionChain chain;
    int value = 0;

    chain.addAction(
        [&]()
        {
            value = 100;
        });

    chain.execute();
    EXPECT_EQ(value, 100);
    EXPECT_TRUE(chain.isCommitted());
}

TEST(ActionChainTest, ExceptionInFirstAction)
{
    ActionChain chain;
    int value = 0;

    chain.addAction(
        [&]()
        {
            throw std::runtime_error("First action failed");
        },
        [&]()
        {
            value = -1;
        });

    chain.addAction(
        [&]()
        {
            value = 42;
        },
        [&]()
        {
            value = 0;
        });

    EXPECT_THROW(chain.execute(), std::runtime_error);
    EXPECT_EQ(value, 0);
    EXPECT_FALSE(chain.isCommitted());
}

TEST(ActionChainTest, ExceptionInSecondAction)
{
    ActionChain chain;
    int value = 0;
    bool firstRolledBack = false;
    bool secondRolledBack = false;

    chain.addAction(
        [&]()
        {
            value = 10;
        },
        [&]()
        {
            value = 0;
            firstRolledBack = true;
        });

    chain.addAction(
        [&]()
        {
            throw std::runtime_error("Second action failed");
        },
        [&]()
        {
            secondRolledBack = true;
        });

    EXPECT_THROW(chain.execute(), std::runtime_error);
    EXPECT_EQ(value, 0);
    EXPECT_TRUE(firstRolledBack);
    EXPECT_FALSE(secondRolledBack);
    EXPECT_FALSE(chain.isCommitted());
}

TEST(ActionChainTest, ExceptionInRollback)
{
    ActionChain chain;
    int value = 0;
    bool rollbackCalled = false;

    chain.addAction(
        [&]()
        {
            value = 42;
        },
        [&]()
        {
            rollbackCalled = true;
            throw std::runtime_error("Rollback failed");
        });

    chain.addAction(
        [&]()
        {
            throw std::runtime_error("Action failed");
        },
        [&]()
        {
        });

    EXPECT_THROW(chain.execute(), std::runtime_error);
    EXPECT_TRUE(rollbackCalled);
    EXPECT_EQ(value, 42);
    EXPECT_FALSE(chain.isCommitted());
}

TEST(ActionChainTest, NestedChainSuccess)
{
    ActionChain outer;
    int outerValue = 0;
    int innerValue = 0;

    outer.addAction(
        [&]()
        {
            ActionChain inner;
            inner.addAction(
                [&]()
                {
                    innerValue = 100;
                },
                [&]()
                {
                    innerValue = 0;
                });
            inner.addAction(
                [&]()
                {
                    innerValue += 50;
                },
                [&]()
                {
                    innerValue -= 50;
                });

            inner.execute();
            inner.commit();
            outerValue = innerValue;
        },
        [&]()
        {
            outerValue = -1;
        });

    outer.execute();
    EXPECT_EQ(innerValue, 150);
    EXPECT_EQ(outerValue, 150);
    EXPECT_TRUE(outer.isCommitted());
}

TEST(ActionChainTest, NestedChainWithException)
{
    ActionChain outer;
    int outerValue = 0;
    bool outerRollback = false;

    outer.addAction(
        [&]()
        {
            ActionChain inner;
            int innerValue = 0;

            inner.addAction(
                [&]()
                {
                    innerValue = 10;
                },
                [&]()
                {
                    innerValue = 0;
                });

            inner.addAction(
                [&]()
                {
                    throw std::runtime_error("Inner action failed");
                },
                [&]()
                {
                });

            try
            {
                inner.execute();
                inner.commit();
            }
            catch (...)
            {
                outerValue = -1;
                throw;
            }
        },
        [&]()
        {
            outerRollback = true;
        });

    EXPECT_THROW(outer.execute(), std::runtime_error);
    EXPECT_FALSE(outerRollback);
    EXPECT_EQ(outerValue, -1);
    EXPECT_FALSE(outer.isCommitted());
}

TEST(ActionChainTest, DeepNestedChains)
{
    ActionChain outer;
    int result = 0;

    outer.addAction(
        [&]()
        {
            ActionChain middle;
            middle.addAction(
                [&]()
                {
                    ActionChain inner;
                    inner.addAction(
                        [&]()
                        {
                            result += 10;
                        },
                        [&]()
                        {
                            result -= 10;
                        });
                    inner.addAction(
                        [&]()
                        {
                            result += 20;
                        },
                        [&]()
                        {
                            result -= 20;
                        });
                    inner.execute();
                    inner.commit();
                },
                [&]()
                {
                    result -= 30;
                });
            middle.execute();
            middle.commit();
        },
        [&]()
        {
            result = -1;
        });

    outer.execute();
    EXPECT_EQ(result, 30);
    EXPECT_TRUE(outer.isCommitted());
}

TEST(ActionChainTest, MoveSemantics)
{
    ActionChain chain1;
    int value = 0;

    chain1.addAction(
        [&]()
        {
            value = 42;
        },
        [&]()
        {
            value = 0;
        });

    ActionChain chain2 = std::move(chain1);
    EXPECT_EQ(chain2.size(), 1);
    EXPECT_EQ(chain1.size(), 0);
    EXPECT_FALSE(chain1.isCommitted());

    chain2.execute();
    EXPECT_EQ(value, 42);
    EXPECT_TRUE(chain2.isCommitted());
}

TEST(ActionChainTest, DestructorRollbackWhenException)
{
    int value = 0;

    {
        ActionChain chain;
        chain.addAction(
            [&]()
            {
                value = 42;
            },
            [&]()
            {
                value = 0;
            });

        chain.addAction(
            [&]()
            {
                throw std::runtime_error("Error");
            },
            [&]()
            {
            });

        try
        {
            chain.execute();
        }
        catch (...)
        {
            // Ignore
        }
    }
    EXPECT_EQ(value, 0);
}

TEST(ActionChainTest, DestructorNoRollbackAfterCommit)
{
    int value = 0;

    {
        ActionChain chain;
        chain.addAction(
            [&]()
            {
                value = 42;
            },
            [&]()
            {
                value = 0;
            });
        chain.execute();
        chain.commit();
    }

    EXPECT_EQ(value, 42);
}

TEST(ActionChainTest, DoubleExecute)
{
    ActionChain chain;
    int value = 0;

    chain.addAction(
        [&]()
        {
            value++;
        },
        [&]()
        {
            value--;
        });

    chain.execute();
    EXPECT_EQ(value, 1);
    EXPECT_TRUE(chain.isCommitted());

    chain.execute();
    EXPECT_EQ(value, 1);
    EXPECT_TRUE(chain.isCommitted());
}

TEST(ActionChainTest, Clear)
{
    ActionChain chain;
    int value = 0;

    chain.addAction(
        [&]()
        {
            value = 42;
        },
        [&]()
        {
            value = 0;
        });
    chain.execute();
    EXPECT_EQ(value, 42);

    chain.clear();
    EXPECT_EQ(chain.size(), 0);
    EXPECT_EQ(value, 0);
    EXPECT_FALSE(chain.isCommitted());
}

TEST(ActionChainTest, ManualRollback)
{
    ActionChain chain;
    int value = 0;

    chain.addAction(
        [&]()
        {
            value = 42;
        },
        [&]()
        {
            value = 0;
        });

    chain.execute();
    EXPECT_EQ(value, 42);
    EXPECT_TRUE(chain.isCommitted());

    chain.rollback();
    EXPECT_EQ(value, 0);
    EXPECT_FALSE(chain.isCommitted());
}

TEST(ActionChainTest, PartialRollbackWithMultipleActions)
{
    ActionChain chain;
    std::vector<int> values;

    chain.addAction(
        [&]()
        {
            values.push_back(1);
        },
        [&]()
        {
            values.pop_back();
        });

    chain.addAction(
        [&]()
        {
            values.push_back(2);
        },
        [&]()
        {
            values.pop_back();
        });

    chain.addAction(
        [&]()
        {
            values.push_back(3);
        },
        [&]()
        {
            values.pop_back();
        });

    chain.addAction(
        [&]()
        {
            throw std::runtime_error("Failed");
        },
        [&]()
        {
        });

    EXPECT_THROW(chain.execute(), std::runtime_error);
    EXPECT_TRUE(values.empty());
    EXPECT_FALSE(chain.isCommitted());
}

TEST(ActionChainTest, MixedRollbacks)
{
    ActionChain chain;
    int a = 0, b = 0, c = 0;

    chain.addAction(
        [&]()
        {
            a = 1;
        },
        [&]()
        {
            a = 0;
        });

    chain.addAction(
        [&]()
        {
            b = 2;
        },
        [&]()
        {
            b = 0;
        });

    chain.addAction(
        [&]()
        {
            throw std::runtime_error("Error");
        },
        [&]()
        {
            c = -1;
        });

    chain.addAction(
        [&]()
        {
            c = 3;
        },
        [&]()
        {
            c = 0;
        });

    EXPECT_THROW(chain.execute(), std::runtime_error);
    EXPECT_EQ(a, 0);
    EXPECT_EQ(b, 0);
    EXPECT_EQ(c, 0);
    EXPECT_FALSE(chain.isCommitted());
}

TEST(ActionChainTest, ExceptionSafety)
{
    bool destructorCalled = false;

    struct TestResource
    {
        bool& flag;
        TestResource(bool& f)
            : flag(f)
        {
        }
        ~TestResource()
        {
            flag = true;
        }
    };

    {
        ActionChain chain;

        chain.addAction(
            [resource = std::make_shared<TestResource>(destructorCalled)]()
            {
                throw std::runtime_error("Error");
            },
            []()
            {
            });

        EXPECT_THROW(chain.execute(), std::runtime_error);
    }

    EXPECT_TRUE(destructorCalled);
}
