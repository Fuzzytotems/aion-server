// P4-11b ObserveController and observer bodies: ObserverType mask matching, notification dispatch by type, one-time observers, removal with and
// without onRemoved, item use aborts, attack calc observers, the C++-only breakers, and add/remove/notify from concurrent threads with every
// observer destroyed afterwards. Expectations are derived by hand from ObserveController.java, ObserverType.java, ActionObserver.java,
// AttackCalcObserver.java and AttackerCriticalStatusObserver.java.

#include "ControllersTestSupport.h"

#include <atomic>
#include <thread>
#include <vector>

#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/attack/AttackStatus.h"
#include "aion/gameserver/controllers/observer/ActionObserver.h"
#include "aion/gameserver/controllers/observer/AttackCalcObserver.h"
#include "aion/gameserver/controllers/observer/AttackStatusObserver.h"
#include "aion/gameserver/controllers/observer/AttackerCriticalStatus.h"
#include "aion/gameserver/controllers/observer/AttackerCriticalStatusObserver.h"
#include "aion/gameserver/controllers/observer/DeathObserver.h"
#include "aion/gameserver/controllers/observer/ItemUseObserver.h"
#include "aion/gameserver/controllers/observer/ObserverType.h"
#include "aion/gameserver/controllers/observer/ObserverTypeInfo.h"
#include "aion/gameserver/controllers/observer/StartMovingListener.h"
#include "aion/gameserver/skillengine/effect/AbnormalState.h"

namespace aion::gameserver::controllers::testing {
namespace {

using attack::AttackStatus;
using model::gameobjects::Creature;
using observer::ObserverType;
using runtime::Ptr;
using runtime::Ref;

std::atomic<int32_t> destroyedObservers{0};

/** Records every notification it receives (Java: an anonymous ActionObserver overriding all hooks). */
class RecordingObserver final : public observer::ActionObserver {
	AION_MAKE_REF_FRIEND
public:
	std::atomic<int32_t> moves{0};
	std::atomic<int32_t> sits{0};
	std::atomic<int32_t> summonReleases{0};
	std::atomic<int32_t> removed{0};
	std::atomic<int32_t> attacks{0};
	std::atomic<int32_t> attackeds{0};
	std::atomic<int32_t> deaths{0};
	std::atomic<int32_t> lastSkillId{-1};
	std::atomic<int32_t> lastHp{-1};
	std::atomic<int32_t> lastCreatureId{-1};
	std::atomic<bool> abnormalSet{false};

	static Ref<RecordingObserver> create(ObserverType type) { return runtime::makeRef<RecordingObserver>(type); }

	void onRemoved() override { removed.fetch_add(1); }
	void moved() override { moves.fetch_add(1); }
	void sit() override { sits.fetch_add(1); }
	void summonrelease() override { summonReleases.fetch_add(1); }
	void hpChanged(int32_t value) override { lastHp = value; }
	void abnormalsetted(skillengine::effect::AbnormalState state) override { abnormalSet = state == skillengine::effect::AbnormalState::STUN; }
	void attack(Creature& creature, int32_t skillId) override {
		attacks.fetch_add(1);
		lastSkillId = skillId;
		lastCreatureId = creature.getObjectId();
	}
	void attacked(Creature& creature, int32_t skillId) override {
		attackeds.fetch_add(1);
		lastSkillId = skillId;
		lastCreatureId = creature.getObjectId();
	}
	void died(Creature& lastAttacker) override {
		deaths.fetch_add(1);
		lastCreatureId = lastAttacker.getObjectId();
	}

protected:
	explicit RecordingObserver(ObserverType type) : ActionObserver(type) {}
	~RecordingObserver() override { destroyedObservers.fetch_add(1); }
};

/** Java: an anonymous ItemUseObserver of an item action */
class RecordingItemUseObserver final : public observer::ItemUseObserver {
	AION_MAKE_REF_FRIEND
public:
	std::atomic<int32_t> aborts{0};
	std::atomic<int32_t> removed{0};

	static Ref<RecordingItemUseObserver> create() { return runtime::makeRef<RecordingItemUseObserver>(); }

	void abort() override { aborts.fetch_add(1); }
	void onRemoved() override { removed.fetch_add(1); }

protected:
	RecordingItemUseObserver() = default;
	~RecordingItemUseObserver() override { destroyedObservers.fetch_add(1); }
};

/** Java: the anonymous AttackStatusObserver of AlwaysDodgeEffect (checkStatus answers DODGE) */
class DodgeObserver final : public observer::AttackStatusObserver {
	AION_MAKE_REF_FRIEND
public:
	static Ref<DodgeObserver> create(float physicalMultiplier) { return runtime::makeRef<DodgeObserver>(physicalMultiplier); }

	bool checkStatus(AttackStatus attackStatus) override { return attackStatus == status.get() && value.get() > 0; }
	float getBasePhysicalDamageMultiplier(bool isSkill) override { return isSkill ? 1.0f : physicalMultiplier; }

protected:
	explicit DodgeObserver(float multiplier) : AttackStatusObserver(1, AttackStatus::DODGE), physicalMultiplier(multiplier) {}
	~DodgeObserver() override = default;

private:
	const float physicalMultiplier;
};

/** Java: the anonymous AttackerCriticalStatusObserver of a critical buff (answers with its own status while its count lasts) */
class CriticalObserver final : public observer::AttackerCriticalStatusObserver {
	AION_MAKE_REF_FRIEND
public:
	static Ref<CriticalObserver> create(int32_t count) { return runtime::makeRef<CriticalObserver>(count); }

	Ref<observer::AttackerCriticalStatus> checkAttackerCriticalStatus(AttackStatus attackStatus, bool isSkill) override {
		if (attackStatus == status.get() && getCount() > 0) {
			decreaseCount();
			Ref<observer::AttackerCriticalStatus> result = observer::AttackerCriticalStatus::create(getCount(), 50, true);
			result->setResult(true);
			return result;
		}
		return AttackerCriticalStatusObserver::checkAttackerCriticalStatus(attackStatus, isSkill);
	}

protected:
	explicit CriticalObserver(int32_t count) : AttackerCriticalStatusObserver(AttackStatus::CRITICAL, count, 50, true) {}
	~CriticalObserver() override = default;
};

/** The DeathObserver action: an unpinned TaskStruct callback recording the last attacker */
struct CountDeaths : runtime::TaskStruct {
	void operator()(Creature& lastAttacker) const { lastDeathId.store(lastAttacker.getObjectId()); }
	static inline std::atomic<int32_t> lastDeathId{0};
};

/** Java: an observer calling getObserveController().removeObserver(this) in moved(), as many handlers do */
class SelfRemovingObserver final : public observer::ActionObserver {
	AION_MAKE_REF_FRIEND
public:
	const Ref<ObserveController> controller;
	std::atomic<int32_t> moves{0};
	static Ref<SelfRemovingObserver> create(ObserveController& c) { return runtime::makeRef<SelfRemovingObserver>(c); }
	void moved() override {
		moves.fetch_add(1);
		controller->removeObserver(*this); // notified outside of the list monitor: no deadlock, no iterator invalidation
	}

protected:
	explicit SelfRemovingObserver(ObserveController& c) : ActionObserver(ObserverType::MOVE), controller(c) {}
	~SelfRemovingObserver() override = default;
};

class ObserveControllerTest : public ControllersTest {
protected:
	void SetUp() override {
		ControllersTest::SetUp();
		destroyedObservers = 0;
	}
};

TEST(ObserverTypeInfoTest, MasksAndMatchingFollowJava) {
	using observer::getObserverMask;
	using observer::matchesObserver;
	EXPECT_EQ(getObserverMask(ObserverType::MOVE), 1);
	EXPECT_EQ(getObserverMask(ObserverType::BOOSTSKILLCOST), 1 << 14);
	EXPECT_EQ(getObserverMask(ObserverType::EQUIP_UNEQUIP), 8 | 16);
	EXPECT_EQ(getObserverMask(ObserverType::DOT_ATTACK_DEFEND), 128 | 2 | 4);
	EXPECT_EQ(getObserverMask(ObserverType::MOVE_OR_DIE), 1 | 64);
	EXPECT_EQ(getObserverMask(ObserverType::ALL), (1 << 15) - 1) << "the 15 single bits";

	// observer.matchesObserver(type): every bit of the notified type must be set in the observer's type
	EXPECT_TRUE(matchesObserver(ObserverType::ALL, ObserverType::SIT));
	EXPECT_TRUE(matchesObserver(ObserverType::MOVE_OR_DIE, ObserverType::DEATH));
	EXPECT_TRUE(matchesObserver(ObserverType::ATTACK_DEFEND, ObserverType::ATTACKED));
	EXPECT_FALSE(matchesObserver(ObserverType::MOVE, ObserverType::ALL)) << "a single-bit observer never matches a compound notification";
	EXPECT_FALSE(matchesObserver(ObserverType::EQUIP_UNEQUIP, ObserverType::MOVE));
	EXPECT_FALSE(matchesObserver(ObserverType::MOVE_OR_DIE, ObserverType::ATTACK));
}

TEST_F(ObserveControllerTest, NotificationsReachObserversOfMatchingTypes) {
	CONTROLLERS_TEST_SCOPE;
	Ref<ObserveController> controller = ObserveController::create();
	Ref<RecordingObserver> mover = RecordingObserver::create(ObserverType::MOVE);
	Ref<RecordingObserver> all = RecordingObserver::create(ObserverType::ALL);
	Ref<RecordingObserver> moveOrDie = RecordingObserver::create(ObserverType::MOVE_OR_DIE);
	controller->addObserver(*mover);
	controller->addObserver(*all);
	controller->addObserver(*moveOrDie);
	EXPECT_TRUE(controller->hasObservers());

	controller->notifyMoveObservers();
	EXPECT_EQ(mover->moves, 1);
	EXPECT_EQ(all->moves, 1);
	EXPECT_EQ(moveOrDie->moves, 1);

	controller->notifySitObservers();
	controller->notifySummonReleaseObservers();
	controller->notifyHPChangeObservers(4321);
	controller->notifyAbnormalSettedObservers(skillengine::effect::AbnormalState::STUN);
	EXPECT_EQ(mover->sits, 0);
	EXPECT_EQ(moveOrDie->sits, 0);
	EXPECT_EQ(all->sits, 1);
	EXPECT_EQ(all->summonReleases, 1);
	EXPECT_EQ(all->lastHp, 4321);
	EXPECT_TRUE(all->abnormalSet);
	EXPECT_EQ(mover->lastHp, -1);

	Ref<ControllersTestNpc> attacker = createNpc();
	controller->notifyAttackObservers(*attacker, 17);
	EXPECT_EQ(all->attacks, 1);
	EXPECT_EQ(all->lastSkillId, 17);
	EXPECT_EQ(all->lastCreatureId, attacker->getObjectId());
	controller->notifyAttackedObservers(*attacker, 0);
	EXPECT_EQ(all->attackeds, 1);
	EXPECT_EQ(all->lastSkillId, 0);
	controller->notifyDeathObservers(*attacker);
	EXPECT_EQ(all->deaths, 1);
	EXPECT_EQ(moveOrDie->deaths, 1);
	EXPECT_EQ(mover->deaths, 0);
	EXPECT_EQ(mover->removed, 0) << "notifications never remove permanent observers";
}

TEST_F(ObserveControllerTest, AttachedObserversAreNotifiedOnceThenRemoved) {
	CONTROLLERS_TEST_SCOPE;
	Ref<ObserveController> controller = ObserveController::create();
	Ref<RecordingObserver> once = RecordingObserver::create(ObserverType::MOVE);
	Ref<RecordingObserver> sitOnce = RecordingObserver::create(ObserverType::SIT);
	controller->attach(*once);
	controller->attach(*sitOnce);
	EXPECT_TRUE(once->isOneTimeUse());

	controller->notifyMoveObservers();
	EXPECT_EQ(once->moves, 1);
	EXPECT_EQ(once->removed, 1) << "onRemoved after the notification";
	EXPECT_EQ(sitOnce->removed, 0) << "not matched: stays attached";

	controller->notifyMoveObservers();
	EXPECT_EQ(once->moves, 1) << "removed from the list before it was notified";
	controller->removeObserver(*once);
	EXPECT_EQ(once->removed, 1) << "removeObserver of an absent observer calls no onRemoved";

	controller->removeObserver(*sitOnce);
	EXPECT_EQ(sitOnce->removed, 1);
	EXPECT_FALSE(controller->hasObservers());
}

TEST_F(ObserveControllerTest, ClearNotifiesButClearWithoutNotifyDoesNot) {
	CONTROLLERS_TEST_SCOPE;
	Ref<ObserveController> controller = ObserveController::create();
	Ref<RecordingObserver> first = RecordingObserver::create(ObserverType::ALL);
	Ref<RecordingObserver> second = RecordingObserver::create(ObserverType::ALL);
	Ref<DodgeObserver> dodge = DodgeObserver::create(0.5f);

	controller->addObserver(*first);
	controller->addAttackCalcObserver(*dodge);
	controller->clear();
	EXPECT_EQ(first->removed, 1);
	EXPECT_FALSE(controller->hasObservers()) << "clear also empties the attack calc observers";
	EXPECT_FALSE(controller->checkAttackStatus(AttackStatus::DODGE));

	controller->addObserver(*second);
	controller->addAttackCalcObserver(*dodge);
	EXPECT_TRUE(controller->hasObservers());
	controller->clearWithoutNotify();
	controller->clearWithoutNotify(); // idempotent
	EXPECT_EQ(second->removed, 0) << "the C++-only breaker never calls onRemoved";
	EXPECT_FALSE(controller->hasObservers());
	controller->notifyMoveObservers();
	EXPECT_EQ(second->moves, 0);
}

TEST_F(ObserveControllerTest, AbortItemUseObserversAbortsOnlyItemUseObservers) {
	CONTROLLERS_TEST_SCOPE;
	Ref<ObserveController> controller = ObserveController::create();
	Ref<RecordingItemUseObserver> itemUse = RecordingItemUseObserver::create();
	Ref<RecordingObserver> other = RecordingObserver::create(ObserverType::ALL);
	controller->addObserver(*itemUse);
	controller->addObserver(*other);

	controller->notifySitObservers();
	EXPECT_EQ(itemUse->aborts, 1) << "ItemUseObserver.sit aborts the item use";
	controller->notifySummonReleaseObservers();
	EXPECT_EQ(itemUse->aborts, 1) << "summonrelease is not overridden by ItemUseObserver";

	controller->abortItemUseObservers();
	EXPECT_EQ(itemUse->removed, 1);
	EXPECT_EQ(itemUse->aborts, 2);
	EXPECT_EQ(other->removed, 0);
	controller->notifyMoveObservers();
	EXPECT_EQ(itemUse->aborts, 2) << "removed from the list";
	EXPECT_EQ(other->moves, 1);
}

TEST_F(ObserveControllerTest, AttackCalcObserversCombineLikeJava) {
	CONTROLLERS_TEST_SCOPE;
	Ref<ObserveController> controller = ObserveController::create();
	EXPECT_FLOAT_EQ(controller->getBasePhysicalDamageMultiplier(false), 1.0f);
	EXPECT_FALSE(controller->checkAttackerCriticalStatus(AttackStatus::CRITICAL, false)->isResult());

	Ref<observer::AttackCalcObserver> neutral = observer::AttackCalcObserver::create();
	Ref<DodgeObserver> half = DodgeObserver::create(0.5f);
	Ref<DodgeObserver> triple = DodgeObserver::create(3.0f);
	Ref<CriticalObserver> critical = CriticalObserver::create(2);
	controller->addAttackCalcObserver(*neutral);
	controller->addAttackCalcObserver(*half);
	controller->addAttackCalcObserver(*triple);
	controller->addAttackCalcObserver(*critical);

	EXPECT_TRUE(controller->checkAttackStatus(AttackStatus::DODGE));
	EXPECT_FALSE(controller->checkAttackStatus(AttackStatus::PARRY));
	EXPECT_FALSE(controller->checkAttackerStatus(AttackStatus::DODGE)) << "the base class answers false";
	EXPECT_FLOAT_EQ(controller->getBasePhysicalDamageMultiplier(false), 1.5f);
	EXPECT_FLOAT_EQ(controller->getBasePhysicalDamageMultiplier(true), 1.0f);
	EXPECT_FLOAT_EQ(controller->getBaseMagicalDamageMultiplier(), 1.0f);

	Ref<observer::AttackerCriticalStatus> first = controller->checkAttackerCriticalStatus(AttackStatus::CRITICAL, false);
	EXPECT_TRUE(first->isResult());
	EXPECT_EQ(first->getValue(), 50);
	EXPECT_TRUE(first->isPercent());
	EXPECT_EQ(critical->getCount(), 1);
	EXPECT_TRUE(controller->checkAttackerCriticalStatus(AttackStatus::CRITICAL, false)->isResult());
	EXPECT_FALSE(controller->checkAttackerCriticalStatus(AttackStatus::CRITICAL, false)->isResult()) << "count used up";
	EXPECT_EQ(critical->getCount(), 0);

	controller->removeAttackCalcObserver(*triple);
	EXPECT_FLOAT_EQ(controller->getBasePhysicalDamageMultiplier(false), 0.5f);
}

TEST_F(ObserveControllerTest, DeathObserverAndStartMovingListener) {
	CONTROLLERS_TEST_SCOPE;
	Ref<ObserveController> controller = ObserveController::create();
	Ref<observer::DeathObserver> death = observer::DeathObserver::create(runtime::PinnedCallback<void(Creature&)>(CountDeaths{}));
	Ref<observer::StartMovingListener> moving = observer::StartMovingListener::create();
	controller->addObserver(*death);
	controller->addObserver(*moving);
	EXPECT_FALSE(moving->isEffectorMoved());

	Ref<ControllersTestNpc> killer = createNpc();
	controller->notifyAttackObservers(*killer, 1);
	EXPECT_EQ(CountDeaths::lastDeathId.load(), 0) << "a DEATH observer ignores attacks";
	controller->notifyDeathObservers(*killer);
	EXPECT_EQ(CountDeaths::lastDeathId.load(), killer->getObjectId());
	controller->notifyMoveObservers();
	EXPECT_TRUE(moving->isEffectorMoved());
}

TEST_F(ObserveControllerTest, ObserverCanRemoveItselfWhileBeingNotified) {
	CONTROLLERS_TEST_SCOPE;
	Ref<ObserveController> controller = ObserveController::create();
	Ref<SelfRemovingObserver> selfRemoving = SelfRemovingObserver::create(*controller);
	Ref<RecordingObserver> after = RecordingObserver::create(ObserverType::MOVE);
	controller->addObserver(*selfRemoving);
	controller->addObserver(*after);
	controller->notifyMoveObservers();
	controller->notifyMoveObservers();
	EXPECT_EQ(selfRemoving->moves, 1);
	EXPECT_EQ(after->moves, 2);
}

TEST_F(ObserveControllerTest, ConcurrentAddNotifyRemoveKeepsJavaSemanticsAndLeavesNoObservers) {
	constexpr int adders = 4;
	constexpr int notifiers = 4;
	constexpr int perThread = 300;
	Ref<ObserveController> controller;
	{
		CONTROLLERS_TEST_SCOPE;
		controller = ObserveController::create();
	}
	std::vector<std::vector<Ref<RecordingObserver>>> attached(adders);
	std::atomic<bool> go{false};
	std::atomic<bool> addersDone{false};
	std::vector<std::thread> workers;
	for (int t = 0; t < adders; ++t) {
		workers.emplace_back([&, t] {
			while (!go.load())
				std::this_thread::yield();
			for (int i = 0; i < perThread; ++i) {
				CONTROLLERS_TEST_SCOPE;
				Ref<RecordingObserver> oneTime = RecordingObserver::create(ObserverType::MOVE);
				controller->attach(*oneTime);
				attached[static_cast<size_t>(t)].push_back(oneTime);
				Ref<RecordingObserver> permanent = RecordingObserver::create(ObserverType::ALL);
				controller->addObserver(*permanent);
				controller->removeObserver(*permanent);
				EXPECT_EQ(permanent->removed, 1) << "exactly one onRemoved for an added and removed observer";
			}
		});
	}
	for (int t = 0; t < notifiers; ++t) {
		workers.emplace_back([&] {
			while (!go.load())
				std::this_thread::yield();
			while (!addersDone.load()) {
				CONTROLLERS_TEST_SCOPE;
				controller->notifyMoveObservers();
			}
		});
	}
	go = true;
	for (int t = 0; t < adders; ++t)
		workers[static_cast<size_t>(t)].join();
	addersDone = true;
	for (size_t t = adders; t < workers.size(); ++t)
		workers[t].join();
	{
		CONTROLLERS_TEST_SCOPE;
		controller->notifyMoveObservers(); // delivers the one-time observers the notifiers missed
		EXPECT_FALSE(controller->hasObservers());
	}
	for (const std::vector<Ref<RecordingObserver>>& list : attached) {
		for (const Ref<RecordingObserver>& oneTime : list) {
			EXPECT_EQ(oneTime->moves, 1) << "a one-time observer is removed under the list monitor, so exactly one notifier delivers it";
			EXPECT_EQ(oneTime->removed, 1);
		}
	}
	attached.clear();
	controller.reset();
	runtime::Reclaimer::getInstance().drain();
	EXPECT_EQ(destroyedObservers.load(), adders * perThread * 2) << "no observer is kept alive by the controller or a notification";
}

} // namespace
} // namespace aion::gameserver::controllers::testing
