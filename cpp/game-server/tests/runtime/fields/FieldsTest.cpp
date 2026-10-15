// Field<T> (scalars, Ref, std::string, shared_ptr) and Final<T> (design §3.2.3, §2.4 read barrier, §12.1 torn scalars / immutable boxes).

#include <gtest/gtest.h>

#include <atomic>
#include <cstring>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "sync/LockdepTestSupport.h"
#include "sync/SyncTestSupport.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/fields/Final.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"

using namespace aion::gameserver::runtime;
using namespace aion::gameserver::runtime::testsupport;
using namespace std::chrono_literals;

AION_LOCKDEP_FAIL_TESTS_ON_CYCLES();

namespace {

class Npc final : public RefCounted {
	AION_MAKE_REF_FRIEND

public:
	static Ref<Npc> create(int32_t id) { return makeRef<Npc>(id); }
	/** a checksum of the constant id, readable after the object was replaced in a field */
	bool intact() const noexcept { return id * 7 + 3 == check; }
	int32_t getId() const noexcept { return id; }

protected:
	explicit Npc(int32_t id) : id(id), check(id * 7 + 3) {}
	~Npc() override { check = -1; }

private:
	const int32_t id;
	int32_t check;
};

struct Position {
	float x, y, z;
	uint8_t heading;
	friend bool operator==(const Position&, const Position&) = default;
};

enum class AIState : uint8_t { IDLE, FIGHT, DIED };

class Template {
public:
	int32_t value = 42;
};

class Connection {
public:
	int32_t id = 7;
};

} // namespace

TEST(FieldTest, ScalarOperationsFollowJava) {
	Field<int32_t> counter{5};
	EXPECT_EQ(counter += 3, 8);
	EXPECT_EQ(counter -= 1, 7);
	EXPECT_EQ(counter *= 2, 14);
	EXPECT_EQ(counter /= 7, 2);
	EXPECT_EQ(counter |= 8, 10);
	EXPECT_EQ(counter &= 6, 2);
	EXPECT_EQ(counter ^= 3, 1);
	EXPECT_EQ(counter++, 1);
	EXPECT_EQ(++counter, 3);
	EXPECT_EQ(counter--, 3);
	EXPECT_EQ(--counter, 1);
	EXPECT_EQ(counter.exchange(9), 1);
	EXPECT_FALSE(counter.compareAndSet(1, 2));
	EXPECT_TRUE(counter.compareAndSet(9, 2));
	int32_t loaded = counter;
	EXPECT_EQ(loaded, 2);

	Field<float> hp{1.5f};
	hp += 1.0f;
	EXPECT_FLOAT_EQ(hp.get(), 2.5f);
	Field<bool> flag;
	EXPECT_FALSE(flag.get());
	flag = true;
	EXPECT_TRUE(flag);

	Field<AIState> state{AIState::IDLE};
	EXPECT_TRUE(state.compareAndSet(AIState::IDLE, AIState::FIGHT));
	EXPECT_EQ(state.get(), AIState::FIGHT);

	Field<std::optional<int64_t>> lastAttack;
	EXPECT_FALSE(lastAttack.get().has_value());
	lastAttack = std::optional<int64_t>(123);
	EXPECT_EQ(lastAttack.get(), std::optional<int64_t>(123));

	static const Template tpl;
	Field<const Template*> templateField;
	EXPECT_EQ(templateField.get(), nullptr);
	templateField = &tpl;
	EXPECT_EQ(templateField->value, 42);
}

TEST(FieldTest, AggregateScalarsAreNeverTorn) {
	HangGuard guard(60s);
	Field<Position> position{Position{1, 1, 1, 1}};
	std::atomic<bool> stop{false};
	std::atomic<bool> torn{false};
	std::vector<std::thread> threads;
	for (int w = 0; w < 2; ++w)
		threads.emplace_back([&, w] {
			for (int i = 0; !stop.load(); ++i) {
				float v = static_cast<float>((i % 1000) + w * 1000);
				position = Position{v, v, v, static_cast<uint8_t>(static_cast<int>(v) % 256)};
			}
		});
	for (int r = 0; r < 2; ++r)
		threads.emplace_back([&] {
			while (!stop.load()) {
				Position p = position;
				if (p.x != p.y || p.y != p.z || p.heading != static_cast<uint8_t>(static_cast<int>(p.x) % 256))
					torn = true;
			}
		});
	std::this_thread::sleep_for(200ms);
	stop = true;
	for (std::thread& thread : threads)
		thread.join();
	EXPECT_FALSE(torn.load());
}

TEST(FieldTest, RefFieldLoadIsAReadBarrierThatPublishes) {
	Ref<Npc> npc = Npc::create(1);
	Field<Ref<Npc>> boss{npc};
	{
		TaskScope scope(AION_TASK_INFO(TaskKind::TEST));
		EXPECT_FALSE(TaskScope::isPublished()) << "lazy publication: entering a scope does not publish";
		Ptr<Npc> loaded = boss.get();
		EXPECT_TRUE(TaskScope::isPublished());
		EXPECT_EQ(loaded, npc);
		EXPECT_EQ(boss->getId(), 1);
		EXPECT_EQ((*boss).getId(), 1);
	}
	for (auto load : std::vector<std::function<void()>>{
			 [&] { (void)static_cast<bool>(boss); },
			 [&] { (void)boss->getId(); },
			 [&] { Ptr<Npc> converted = boss; },
			 [&] { (void)(boss == Ptr<Npc>(npc)); },
		 }) {
		TaskScope scope(AION_TASK_INFO(TaskKind::TEST));
		ASSERT_FALSE(TaskScope::isPublished());
		load();
		EXPECT_TRUE(TaskScope::isPublished());
	}
	TaskScope scope(AION_TASK_INFO(TaskKind::TEST));
	Field<Ref<Npc>> empty;
	EXPECT_FALSE(empty);
	EXPECT_TRUE(empty == nullptr);
	EXPECT_THROW((void)empty->getId(), NullPointerException);
	EXPECT_THROW((void)*empty, NullPointerException);
}

TEST(FieldTest, RefFieldStoresExchangesAndComparesByIdentity) {
	TaskScope scope(AION_TASK_INFO(TaskKind::TEST));
	Ref<Npc> first = Npc::create(1);
	Ref<Npc> second = Npc::create(2);
	{
		Field<Ref<Npc>> target;
		target = first;
		EXPECT_EQ(first->refCount(), 2u);
		target = Ptr<Npc>(second);
		EXPECT_EQ(first->refCount(), 1u);
		EXPECT_EQ(second->refCount(), 2u);
		Ref<Npc> previous = target.exchange(first);
		EXPECT_EQ(previous, second);
		EXPECT_EQ(second->refCount(), 2u); // previous holds it
		previous.reset();
		EXPECT_FALSE(target.compareAndSet(Ptr<Npc>(second), second));
		EXPECT_EQ(second->refCount(), 1u) << "a failed compareAndSet drops the new value";
		EXPECT_TRUE(target.compareAndSet(Ptr<Npc>(first), second));
		EXPECT_EQ(first->refCount(), 1u);
		EXPECT_TRUE(target.compareAndSet(Ptr<Npc>(second), second)); // same object
		EXPECT_EQ(second->refCount(), 2u);
		target.set(nullptr);
		EXPECT_EQ(second->refCount(), 1u);
		EXPECT_TRUE(target.compareAndSet(nullptr, first));
		EXPECT_EQ(first->refCount(), 2u);
	}
	EXPECT_EQ(first->refCount(), 1u) << "the field destructor releases";
	EXPECT_EQ(second->refCount(), 1u);
}

TEST(FieldTest, StringFieldReadBarrierNullAndEquality) {
	Field<std::string> name{"Poison"};
	{
		TaskScope scope(AION_TASK_INFO(TaskKind::TEST));
		EXPECT_FALSE(TaskScope::isPublished());
		const std::string& borrowed = name.get();
		EXPECT_TRUE(TaskScope::isPublished());
		name = "Burn";
		EXPECT_EQ(borrowed, "Poison") << "a borrow stays valid until the task ends, even after a store";
		EXPECT_TRUE(name == "Burn");
		name = nullptr;
		EXPECT_TRUE(name.get().empty()) << "Java null is the empty string";
		name.set("");
		EXPECT_TRUE(name == "");
		std::string copy = name;
		EXPECT_TRUE(copy.empty());
	}
	Reclaimer::getInstance().drain();
}

TEST(FieldTest, StringAndRefFieldsAreTornAndUseAfterFreeFreeUnderConcurrentWrites) {
	// writers replace strings whose content is determined by their length and objects with a checksum; readers in task scopes verify every
	// load; the Reclaimer runs concurrently. Under ASan any use of a freed box or object fails the test.
	HangGuard guard(120s);
	Field<std::string> text;
	Field<Ref<Npc>> npc{Npc::create(0)};
	std::atomic<bool> stop{false};
	std::atomic<bool> corrupt{false};
	std::atomic<int64_t> reads{0};
	std::vector<std::thread> threads;
	for (int w = 0; w < 2; ++w)
		threads.emplace_back([&, w] {
			TaskScope scope(AION_TASK_INFO(TaskKind::TEST));
			for (int i = 0; !stop.load(); ++i) {
				size_t length = static_cast<size_t>((i * 37 + w) % 200);
				text = std::string(length, static_cast<char>('a' + length % 26));
				npc = Npc::create(i);
			}
		});
	for (int r = 0; r < 3; ++r)
		threads.emplace_back([&] {
			while (!stop.load()) {
				TaskScope scope(AION_TASK_INFO(TaskKind::TEST));
				for (int i = 0; i < 100; ++i) {
					const std::string& value = text.get();
					for (char c : value)
						if (c != static_cast<char>('a' + value.size() % 26))
							corrupt = true;
					Ptr<Npc> loaded = npc.get();
					if (!loaded || !loaded->intact())
						corrupt = true;
					reads.fetch_add(1, std::memory_order_relaxed);
				}
			}
		});
	threads.emplace_back([&] {
		while (!stop.load()) {
			Reclaimer::getInstance().reclaimNow();
			std::this_thread::sleep_for(1ms);
		}
	});
	std::this_thread::sleep_for(500ms);
	stop = true;
	for (std::thread& thread : threads)
		thread.join();
	EXPECT_FALSE(corrupt.load());
	EXPECT_GT(reads.load(), 0);
	npc = nullptr;
	text = "";
	EXPECT_TRUE(Reclaimer::getInstance().drain());
}

TEST(FieldTest, SharedPtrFieldIsAtomic) {
	auto connection = std::make_shared<Connection>();
	Field<std::shared_ptr<Connection>> field;
	EXPECT_FALSE(field);
	field = connection;
	EXPECT_EQ(field.get()->id, 7);
	std::shared_ptr<Connection> previous = field.exchange(nullptr);
	EXPECT_EQ(previous, connection);
	EXPECT_TRUE(field.compareAndSet(nullptr, connection));
	EXPECT_FALSE(field.compareAndSet(nullptr, connection));
	field = nullptr;
	EXPECT_EQ(connection.use_count(), 2); // connection + previous
}

TEST(FinalTest, SingleAssignment) {
	Final<int32_t> fromInitializer{5};
	EXPECT_EQ(fromInitializer.get(), 5);
	Final<const Template*> late;
	static const Template tpl;
	late.set(&tpl);
	EXPECT_EQ(late->value, 42);
	EXPECT_EQ((*late).value, 42);
	const Template* raw = late;
	EXPECT_EQ(raw, &tpl);

	Ref<Npc> owner = Npc::create(3);
	Final<int32_t> ownerChecked;
	ownerChecked.set(9, *owner); // count == 1: not yet published
	EXPECT_EQ(ownerChecked.get(), 9);
}

#if AION_CHECKED
TEST(FinalDeathTest, SecondAssignmentAndEarlyReadTerminate) {
	GTEST_FLAG_SET(death_test_style, "threadsafe");
	EXPECT_DEATH(
		{
			Final<int32_t> value;
			value.set(1);
			value.set(2);
		},
		"C11");
	EXPECT_DEATH(
		{
			Final<int32_t> value;
			(void)value.get();
		},
		"C11");
	EXPECT_DEATH(
		{
			Ref<Npc> owner = Npc::create(1);
			Ref<Npc> published = owner;
			Final<int32_t> value;
			value.set(1, *owner);
		},
		"C11");
}
#endif
