#include <gtest/gtest.h>
#include <vector>
#include <string>
#include <memory>
#include <casket/types/fixed_object_pool.hpp>

using namespace casket;

class Trackable
{
public:
    static int constructions;
    static int destructions;
    static int copies;
    static int moves;

    int value;

    Trackable()
        : value(0)
    {
        constructions++;
    }

    explicit Trackable(int v)
        : value(v)
    {
        constructions++;
    }

    Trackable(const Trackable& other)
        : value(other.value)
    {
        copies++;
        constructions++;
    }

    Trackable(Trackable&& other) noexcept
        : value(other.value)
    {
        moves++;
        constructions++;
        other.value = -1;
    }

    ~Trackable()
    {
        destructions++;
    }

    static void resetCounters()
    {
        constructions = 0;
        destructions = 0;
        copies = 0;
        moves = 0;
    }
};

int Trackable::constructions = 0;
int Trackable::destructions = 0;
int Trackable::copies = 0;
int Trackable::moves = 0;

TEST(FixedObjectPoolTest, ConstructorAndDestructor)
{
    Trackable::resetCounters();
    {
        FixedObjectPool<Trackable> pool(5);
        EXPECT_EQ(pool.poolSize(), 5);
        EXPECT_EQ(pool.capacity(), 5);
        EXPECT_EQ(Trackable::constructions, 5);
        EXPECT_EQ(Trackable::destructions, 0);
    }
    EXPECT_EQ(Trackable::destructions, 5);
}

TEST(FixedObjectPoolTest, AcquireAndRelease)
{
    FixedObjectPool<std::string> pool(3);

    std::string* s1 = pool.acquire();
    ASSERT_NE(s1, nullptr);
    *s1 = "hello";

    std::string* s2 = pool.acquire();
    ASSERT_NE(s2, nullptr);
    *s2 = "world";

    std::string* s3 = pool.acquire();
    ASSERT_NE(s3, nullptr);
    *s3 = "test";

    std::string* s4 = pool.acquire();
    EXPECT_EQ(s4, nullptr);

    pool.release(s2);

    std::string* s5 = pool.acquire();
    ASSERT_NE(s5, nullptr);
    EXPECT_EQ(*s5, "world");
}

TEST(FixedObjectPoolTest, AcquireWithArgs)
{
    FixedObjectPool<std::vector<int>> pool(3);

    std::vector<int>* v1 = pool.acquire(5, 42);
    ASSERT_NE(v1, nullptr);
    EXPECT_EQ(v1->size(), 5);
    EXPECT_EQ((*v1)[0], 42);

    std::vector<int>* v2 = pool.acquire(std::initializer_list<int>{1, 2, 3});
    ASSERT_NE(v2, nullptr);
    EXPECT_EQ(v2->size(), 3);
    EXPECT_EQ((*v2)[1], 2);
}

TEST(FixedObjectPoolTest, ReleaseInvalidPointer)
{
    FixedObjectPool<int> pool(3);

    int x = 42;
    pool.release(&x);

    auto* i1 = pool.acquire();
    ASSERT_NE(i1, nullptr);
    *i1 = 100;

    pool.release(i1);
    pool.release(nullptr);
}

TEST(FixedObjectPoolTest, MultipleAcquireRelease)
{
    FixedObjectPool<int> pool(2);

    int* a = pool.acquire();
    int* b = pool.acquire();

    *a = 10;
    *b = 20;

    pool.release(a);
    pool.release(b);

    int* c = pool.acquire();
    int* d = pool.acquire();

    EXPECT_EQ(*c, 20);
    EXPECT_EQ(*d, 10);

    *c = 30;
    *d = 40;

    pool.release(c);
    pool.release(d);

    int* e = pool.acquire();
    int* f = pool.acquire();

    EXPECT_EQ(*e, 40);
    EXPECT_EQ(*f, 30);
}

TEST(FixedObjectPoolTest, ResetBasic)
{
    FixedObjectPool<std::vector<int>> pool(3);

    auto* v1 = pool.acquire();
    v1->push_back(1);
    v1->push_back(2);

    auto* v2 = pool.acquire();
    v2->push_back(3);

    auto* v3 = pool.acquire();
    v3->push_back(4);

    EXPECT_EQ(v1->size(), 2);
    EXPECT_EQ(v2->size(), 1);
    EXPECT_EQ(v3->size(), 1);

    pool.reset();

    auto* v4 = pool.acquire();
    ASSERT_NE(v4, nullptr);
    EXPECT_FALSE(v4->empty());

    auto* v5 = pool.acquire();
    EXPECT_FALSE(v5->empty());

    auto* v6 = pool.acquire();
    EXPECT_FALSE(v6->empty());

    auto* v7 = pool.acquire();
    EXPECT_EQ(v7, nullptr);
}

TEST(FixedObjectPoolTest, ResetIdempotency)
{
    FixedObjectPool<int> pool(5);

    pool.reset();

    int* i1 = pool.acquire();
    ASSERT_NE(i1, nullptr);
    *i1 = 100;

    EXPECT_NO_THROW(pool.reset());

    int* i2 = pool.acquire();
    ASSERT_NE(i2, nullptr);
    EXPECT_EQ(*i2, 100);
}

TEST(FixedObjectPoolTest, ResetWithTrackable)
{
    Trackable::resetCounters();
    {
        FixedObjectPool<Trackable> pool(3);

        auto* t1 = pool.acquire(10);
        auto* t2 = pool.acquire(20);
        auto* t3 = pool.acquire(30);

        EXPECT_EQ(Trackable::constructions, 6);
        EXPECT_EQ(Trackable::destructions, 3);

        EXPECT_EQ(t1->value, 10);
        EXPECT_EQ(t2->value, 20);
        EXPECT_EQ(t3->value, 30);

        pool.release(t2);

        pool.reset();

        EXPECT_EQ(Trackable::constructions, 6);
        EXPECT_EQ(Trackable::destructions, 3);

        auto* t4 = pool.acquire();
        ASSERT_NE(t4, nullptr);
        EXPECT_EQ(t4->value, 10);
    }
    EXPECT_EQ(Trackable::destructions, 6);
}

TEST(FixedObjectPoolTest, ResetWithTrackableNoArgs)
{
    Trackable::resetCounters();
    {
        FixedObjectPool<Trackable> pool(3);
        
        EXPECT_EQ(Trackable::constructions, 3);
        EXPECT_EQ(Trackable::destructions, 0);
        
        auto* t1 = pool.acquire();
        auto* t2 = pool.acquire();
        auto* t3 = pool.acquire();
        
        EXPECT_EQ(Trackable::constructions, 3);
        EXPECT_EQ(Trackable::destructions, 0);
        
        EXPECT_EQ(t1->value, 0);
        EXPECT_EQ(t2->value, 0);
        EXPECT_EQ(t3->value, 0);
        
        t1->value = 10;
        t2->value = 20;
        t3->value = 30;
        
        pool.release(t2);
        
        pool.reset();
        EXPECT_EQ(Trackable::destructions, 0);

        auto* t4 = pool.acquire();
        ASSERT_NE(t4, nullptr);
        EXPECT_EQ(t4->value, 10);
        
        auto* t5 = pool.acquire();
        EXPECT_EQ(t5->value, 20);
        
        auto* t6 = pool.acquire();
        EXPECT_EQ(t6->value, 30);
        
        auto* t7 = pool.acquire();
        EXPECT_EQ(t7, nullptr);
    }
    EXPECT_EQ(Trackable::destructions, 3);
}

TEST(FixedObjectPoolTest, ResetWithPartialRelease)
{
    FixedObjectPool<std::string> pool(4);

    auto* s1 = pool.acquire("first");
    ASSERT_NE(s1, nullptr);

    auto* s2 = pool.acquire("second");
    ASSERT_NE(s2, nullptr);

    auto* s3 = pool.acquire("third");
    ASSERT_NE(s3, nullptr);
    
    auto* s4 = pool.acquire("fourth");
    ASSERT_NE(s4, nullptr);

    pool.release(s2);
    pool.release(s4);

    pool.reset();

    auto* n1 = pool.acquire();
    auto* n2 = pool.acquire();
    auto* n3 = pool.acquire();
    auto* n4 = pool.acquire();

    EXPECT_FALSE(n1->empty());
    EXPECT_FALSE(n2->empty());
    EXPECT_FALSE(n3->empty());
    EXPECT_FALSE(n4->empty());
}

TEST(FixedObjectPoolTest, MultipleResets)
{
    FixedObjectPool<int> pool(3);

    for (int i = 0; i < 10; ++i)
    {
        EXPECT_NO_THROW(pool.reset());

        int* a = pool.acquire();
        int* b = pool.acquire();
        int* c = pool.acquire();

        ASSERT_NE(a, nullptr);
        ASSERT_NE(b, nullptr);
        ASSERT_NE(c, nullptr);

        *a = i * 100;
        *b = i * 100 + 1;
        *c = i * 100 + 2;

        pool.release(a);
        pool.release(b);
        pool.release(c);
    }
}

TEST(FixedObjectPoolTest, ResetAfterFullUsage)
{
    FixedObjectPool<int> pool(1);

    for (int i = 0; i < 5; ++i)
    {
        int* a = pool.acquire();

        ASSERT_NE(a, nullptr);

        *a = i;

        pool.reset();

        int* b = pool.acquire();
        ASSERT_NE(b, nullptr);
        EXPECT_EQ(*b, i);

        pool.release(b);
    }
}

TEST(FixedObjectPoolTest, ResetWithAcquireAfter)
{
    FixedObjectPool<std::vector<int>> pool(2);

    auto* v1 = pool.acquire();
    v1->push_back(1);

    auto* v2 = pool.acquire();
    v2->push_back(2);
    v2->push_back(2);

    pool.release(v1);

    pool.reset();

    auto* v3 = pool.acquire();
    EXPECT_FALSE(v3->empty());

    auto* v4 = pool.acquire();
    EXPECT_FALSE(v4->empty());

    auto* v5 = pool.acquire();
    EXPECT_EQ(v5, nullptr);
}

TEST(FixedObjectPoolTest, ZeroSizePool)
{
    FixedObjectPool<int> pool(0);

    EXPECT_EQ(pool.poolSize(), 0);
    EXPECT_EQ(pool.capacity(), 0);

    auto* obj = pool.acquire();
    EXPECT_EQ(obj, nullptr);

    int x = 42;
    EXPECT_NO_THROW(pool.release(&x));
    EXPECT_NO_THROW(pool.reset());
}

TEST(FixedObjectPoolTest, ReleaseAfterReset)
{
    FixedObjectPool<int> pool(3);

    auto* i1 = pool.acquire();
    auto* i2 = pool.acquire();

    *i1 = 100;
    *i2 = 200;

    pool.reset();

    EXPECT_NO_THROW(pool.release(i1));
    EXPECT_NO_THROW(pool.release(i2));

    auto* i3 = pool.acquire();
    ASSERT_NE(i3, nullptr);
    EXPECT_EQ(*i3, 200);
}

TEST(FixedObjectPoolTest, AcquireAfterResetWithDifferentArgs)
{
    FixedObjectPool<std::string> pool(2);

    auto* s1 = pool.acquire("hello");
    auto* s2 = pool.acquire("world");

    EXPECT_EQ(*s1, "hello");
    EXPECT_EQ(*s2, "world");

    pool.reset();

    auto* s3 = pool.acquire("new value");
    ASSERT_NE(s3, nullptr);
    EXPECT_EQ(*s3, "new value");
}

TEST(FixedObjectPoolTest, StressTest)
{
    FixedObjectPool<int> pool(10);

    for (int i = 0; i < 1000; ++i)
    {
        int action = rand() % 3;

        switch (action)
        {
        case 0:
        {
            int* obj = pool.acquire(i);
            if (obj)
            {
                EXPECT_EQ(*obj, i);
                pool.release(obj);
            }
            break;
        }
        case 1:
        {
            int* obj = pool.acquire();
            if (obj)
            {
                *obj = i;
                pool.release(obj);
            }
            break;
        }
        case 2:
        {
            pool.reset();
            break;
        }
        }
    }
}

TEST(FixedObjectPoolTest, MoveSemantics)
{
    Trackable::resetCounters();
    FixedObjectPool<Trackable> pool(2);

    Trackable t(100);
    EXPECT_EQ(Trackable::constructions, 3);

    auto* p1 = pool.acquire(std::move(t));
    ASSERT_NE(p1, nullptr);

    EXPECT_EQ(Trackable::moves, 1);
    EXPECT_EQ(p1->value, 100);
}
