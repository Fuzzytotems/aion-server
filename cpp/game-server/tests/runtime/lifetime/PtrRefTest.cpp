// Conformance of Ref/Ptr/cast/as (design §2.1, §2.2): null dereferences, identity, conversions, C1 scope stamps and C8 destructor context.

#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <thread>
#include <type_traits>
#include <unordered_set>

#include "LifetimeTestSupport.h"

using namespace aion::gameserver::runtime;
using namespace aion::gameserver::runtime::lifetimetest;

namespace {

class Base : public RefCounted {
	AION_MAKE_REF_FRIEND

public:
	static Ref<Base> create() { return makeRef<Base>(); }
	int value() const noexcept { return 1; }

protected:
	Base() = default;
	~Base() override = default;
};

class Derived final : public Base {
	AION_MAKE_REF_FRIEND

public:
	static Ref<Derived> create() { return makeRef<Derived>(); }

protected:
	Derived() = default;
	~Derived() override = default;
};

/** Destructor dereferences a Ref (forbidden, C8). */
class BadDestructor final : public RefCounted {
	AION_MAKE_REF_FRIEND

public:
	static Ref<BadDestructor> create(Ref<Base> other) { return makeRef<BadDestructor>(std::move(other)); }

protected:
	explicit BadDestructor(Ref<Base> other) : other(std::move(other)) {}
	~BadDestructor() override { (void)this->other->value(); }

private:
	Ref<Base> other;
};

class Controller final : public OwnedPart {
public:
	explicit Controller(const RefCounted& owner) : OwnedPart(owner) {}
};

class WithPart final : public RefCounted {
	AION_MAKE_REF_FRIEND

public:
	static Ref<WithPart> create() { return makeRef<WithPart>(); }
	const std::unique_ptr<Controller> controller = std::make_unique<Controller>(*this);

protected:
	WithPart() = default;
	~WithPart() override = default;
};

} // namespace

TEST(PtrRefTest, NullDereferencesThrowNullPointerException) {
	TaskScope scope(testTask());
	Ref<Base> nullRef;
	Ptr<Base> nullPtr;
	EXPECT_THROW((void)nullRef->value(), NullPointerException);
	EXPECT_THROW((void)(*nullRef).value(), NullPointerException);
	EXPECT_THROW((void)nullPtr->value(), NullPointerException);
	EXPECT_THROW((void)(*nullPtr).value(), NullPointerException);
	EXPECT_EQ(nullRef.get(), nullptr);
	EXPECT_EQ(nullPtr.get(), nullptr);
	EXPECT_FALSE(nullRef);
	EXPECT_FALSE(nullPtr);
}

TEST(PtrRefTest, IdentityComparisonAndHash) {
	TaskScope scope(testTask());
	Ref<Derived> derived = Derived::create();
	Ref<Base> base = derived;
	Ref<Base> other = Base::create();
	Ptr<Base> borrowed = base;
	EXPECT_TRUE(base == derived);
	EXPECT_FALSE(base == other);
	EXPECT_TRUE(borrowed == derived);
	EXPECT_TRUE(borrowed == base.get());
	EXPECT_TRUE(Ptr<Base>() == nullptr);
	EXPECT_EQ(std::hash<Ref<Base>>()(base), std::hash<Base*>()(derived.get()));
	std::unordered_set<Ref<Base>> set{base, other, base};
	EXPECT_EQ(set.size(), 2u);
}

TEST(PtrRefTest, RefOwnershipOperations) {
	Ref<Base> object = Base::create();
	Ref<Base> copy = object;
	EXPECT_EQ(object->refCount(), 2u);
	Ref<Base> moved = std::move(copy);
	EXPECT_FALSE(copy);
	EXPECT_EQ(object->refCount(), 2u);
	Base* raw = moved.leak();
	EXPECT_EQ(object->refCount(), 2u);
	Ref<Base> adopted = Ref<Base>::adopt(raw);
	EXPECT_EQ(object->refCount(), 2u);
	adopted = nullptr;
	EXPECT_EQ(object->refCount(), 1u);
	Ref<Base> fromReference(*object);
	EXPECT_EQ(object->refCount(), 2u);
	fromReference.swap(adopted);
	EXPECT_FALSE(fromReference);
	EXPECT_EQ(adopted, object);
}

TEST(PtrRefTest, RefToPartRetainsTheOwner) {
	Ref<WithPart> owner = WithPart::create();
	{
		Ref<Controller> part(owner->controller.get());
		EXPECT_EQ(owner->refCount(), 2u);
		Ref<Controller> copy = part;
		EXPECT_EQ(owner->refCount(), 3u);
	}
	EXPECT_EQ(owner->refCount(), 1u);
}

namespace {

struct Unrelated {
	virtual ~Unrelated() = default;
	int64_t padding = 7;
};

/** Non-final intermediate class. */
class Middle : public Base {
	AION_MAKE_REF_FRIEND

public:
	static Ref<Middle> create() { return makeRef<Middle>(); }

protected:
	Middle() = default;
	~Middle() override = default;
};

/** Final leaf whose Base subobject is not at offset 0 (the typeid fast path must adjust the pointer like dynamic_cast). */
class Leaf final : public Unrelated, public Middle {
	AION_MAKE_REF_FRIEND

public:
	static Ref<Leaf> create() { return makeRef<Leaf>(); }

protected:
	Leaf() = default;
	~Leaf() override = default;
};

} // namespace

TEST(PtrRefTest, CastToFinalClassesMatchesDynamicCast) {
	Ref<Base> leaf = Leaf::create();
	Ref<Base> middle = Middle::create();
	Ref<Base> base = Base::create();
	{
		TaskScope scope(testTask());
		Ptr<Leaf> asLeaf = as<Leaf>(leaf);
		ASSERT_TRUE(asLeaf);
		EXPECT_EQ(asLeaf.rawPointer(), dynamic_cast<Leaf*>(leaf.get()));
		EXPECT_EQ(asLeaf->padding, 7);
		EXPECT_EQ(cast<Leaf>(leaf).rawPointer(), asLeaf.rawPointer());
		EXPECT_FALSE(as<Leaf>(middle));
		EXPECT_FALSE(as<Leaf>(base));
		EXPECT_THROW((void)cast<Leaf>(middle), ClassCastException);
		EXPECT_EQ(as<Middle>(leaf).rawPointer(), dynamic_cast<Middle*>(leaf.get())) << "non-final targets use dynamic_cast";
		EXPECT_FALSE(as<Leaf>(Ptr<Base>()));
	}
	leaf.reset();
	middle.reset();
	base.reset();
	drainReclaimer();
}

TEST(PtrRefTest, RefToPartCountsReferencesToThePart) {
	Ref<WithPart> owner = WithPart::create();
	EXPECT_EQ(owner->controller->partRefCount(), 0u);
	{
		Ref<Controller> part(owner->controller.get());
		Ref<Controller> copy = part;
		EXPECT_EQ(owner->controller->partRefCount(), 2u);
	}
	EXPECT_EQ(owner->controller->partRefCount(), 0u);
}

TEST(PtrRefTest, CreatingABorrowFromAnOwnedReferencePublishes) {
	Ref<Base> object = Base::create();
	Ref<Base> empty;
	{
		TaskScope scope(testTask());
		Ptr<Base> null = empty;
		EXPECT_FALSE(TaskScope::isPublished()) << "a null borrow protects nothing";
		Ptr<Base> fromRef = object;
		EXPECT_TRUE(TaskScope::isPublished()) << "Ptr(const Ref&) publishes";
		EXPECT_EQ(fromRef, object);
	}
	{
		TaskScope scope(testTask());
		Ptr<Base> fromReference(*object);
		EXPECT_TRUE(TaskScope::isPublished()) << "Ptr(T&) publishes";
	}
	{
		TaskScope scope(testTask());
		Ptr<Base> fromPointer(object.get());
		EXPECT_TRUE(TaskScope::isPublished()) << "Ptr(T*) publishes";
		Ptr<Base> copy = fromPointer;
		EXPECT_EQ(copy, fromPointer);
	}
	{
		TaskScope scope(testTask());
		Ptr<Derived> cast = as<Derived>(object);
		EXPECT_TRUE(TaskScope::isPublished()) << "as<>(const Ref&) goes through Ptr(const Ref&)";
		EXPECT_FALSE(cast);
	}
	Ptr<Base> outsideScopes = object;
	EXPECT_FALSE(TaskScope::isPublished()) << "outside TaskScopes nothing is published";
	(void)outsideScopes;
	drainReclaimer();
}

TEST(PtrRefTest, CastAndAsKeepIdentityAndStamp) {
	TaskScope scope(testTask());
	Ref<Base> derived = Derived::create();
	Ref<Base> plain = Base::create();
	Ptr<Derived> casted = cast<Derived>(derived);
	EXPECT_EQ(casted.rawPointer(), derived.get());
	EXPECT_THROW((void)cast<Derived>(plain), ClassCastException);
	EXPECT_FALSE(as<Derived>(plain));
	EXPECT_TRUE(as<Derived>(*derived));
	EXPECT_FALSE(cast<Derived>(Ptr<Base>()));
	Base& reference = *derived;
	EXPECT_TRUE(cast<Derived>(reference));
}

TEST(PtrRefTest, BorrowedAndNullableTraits) {
	static_assert(std::is_same_v<Borrowed<Ref<Base>>, Ptr<Base>>);
	static_assert(std::is_same_v<Nullable<Ref<Base>>, Ptr<Base>>);
	static_assert(std::is_same_v<Borrowed<int32_t>, int32_t>);
	static_assert(std::is_same_v<Nullable<int32_t>, std::optional<int32_t>>);
	static_assert(std::is_same_v<Borrowed<const Base*>, const Base*>);
	static_assert(std::is_same_v<Nullable<std::shared_ptr<Base>>, std::shared_ptr<Base>>);
	static_assert(IsRef<Ref<Base>> && !IsRef<Ptr<Base>> && IsPtr<Ptr<Base>>);
	static_assert(Retainable<Base> && Retainable<Controller>);
	static_assert(std::is_constructible_v<Ref<Base>, Base&> && !std::is_convertible_v<Base&, Ref<Base>>, "Ref from T& is explicit");
	static_assert(std::is_convertible_v<Base&, Ptr<Base>>, "Ptr from T& is implicit");
	static_assert(std::is_convertible_v<Ptr<Base>, Ref<Base>>, "Ref from Ptr is implicit");
	SUCCEED();
}

#if AION_CHECKED
TEST(PtrRefTest, EscapedBorrowThrowsInALaterScope) {
	Ref<Base> object = Base::create();
	Ptr<Base> escaped;
	{
		TaskScope scope(testTask());
		escaped = object;
		EXPECT_EQ(escaped->value(), 1);
		Ptr<Base> copy = escaped; // copies keep the original stamp
		EXPECT_EQ(copy->value(), 1);
	}
	TaskScope later(testTask());
	EXPECT_THROW((void)escaped->value(), IllegalStateException);
	EXPECT_THROW((void)escaped.get(), IllegalStateException);
	Ptr<Base> copied = escaped;
	EXPECT_THROW((void)copied->value(), IllegalStateException);
	EXPECT_THROW((void)cast<Base>(escaped)->value(), IllegalStateException);
	EXPECT_EQ(escaped.rawPointer(), object.get()) << "rawPointer is unchecked";
	EXPECT_TRUE(escaped == object) << "comparison is unchecked";
	Ptr<Base> fresh = object; // a new borrow in this scope is fine
	EXPECT_EQ(fresh->value(), 1);
}

TEST(PtrRefTest, BorrowCreatedOutsideScopesIsInvalidInsideOne) {
	Ref<Base> object = Base::create();
	Ptr<Base> outside = object;
	EXPECT_EQ(outside->value(), 1); // stamp 0 == no scope
	TaskScope scope(testTask());
	EXPECT_THROW((void)outside->value(), IllegalStateException);
}

TEST(PtrRefTest, BorrowOnAnotherThreadThrows) {
	Ref<Base> object = Base::create();
	TaskScope scope(testTask());
	Ptr<Base> borrowed = object;
	bool threw = false;
	std::thread([&] {
		TaskScope other(testTask());
		try {
			(void)borrowed->value();
		} catch (const IllegalStateException&) {
			threw = true;
		}
	}).join();
	EXPECT_TRUE(threw);
}

TEST(PtrRefTest, JoinHelperSharesTheSubmittersBorrows) {
	Ref<Base> object = Base::create();
	TaskScope scope(testTask());
	Ptr<Base> borrowed = object;
	uint64_t submitter = TaskScope::currentScopeId();
	int value = 0;
	std::thread([&] {
		TaskScope helper(testTask(), submitter);
		EXPECT_TRUE(TaskScope::isJoinedHelper());
		value = borrowed->value();
	}).join();
	EXPECT_EQ(value, 1);
}

TEST(PtrRefTest, QuiescentPointInvalidatesOlderBorrows) {
	Ref<Base> object = Base::create();
	TaskScope scope(testTask());
	QuiescentScope quiescent;
	Ptr<Base> before = object;
	quiescentPoint();
	EXPECT_THROW((void)before->value(), IllegalStateException);
	Ptr<Base> after = object;
	EXPECT_EQ(after->value(), 1);
}

TEST(PtrRefDeathTest, DereferenceInDestructorContextTerminates) {
	GTEST_FLAG_SET(death_test_style, "threadsafe");
	EXPECT_DEATH(
		{
			Reclaimer::getInstance().drain(128);
			Ref<BadDestructor> bad = BadDestructor::create(Base::create());
			bad.reset();
			Reclaimer::getInstance().drain(128);
		},
		"C8");
}
#endif
