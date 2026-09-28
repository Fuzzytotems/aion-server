// P4-13 bodies around an item and a player: the item stones (ItemStone persistent states, ManaStone modifiers, GodStone activation count,
// IdianStone polish charge), RandomBonusEffect, ChargeInfo conditioning, PlayerStorage with its owning player, Exchange and the broker
// list cache, and the lifetimes of stones and conditioning info (LeakCensus). Expectations are derived by hand from ItemStone.java,
// ManaStone.java, GodStone.java, IdianStone.java, RandomBonusEffect.java, ChargeInfo.java, PlayerStorage.java, Exchange.java,
// DropItem.java and BrokerPlayerCache.java.
//
// Test doubles, each standing in for a body of a later chunk (as in tests/player/PlayerCreationTest.cpp):
// - PlayerPetsDAO.getPlayerPets (P4-14): PetList::setPlayerPetsLoaderForTests returns no pets;
// - the stat containers (P5-01): the player's game and life stats are CreatureGameStats/CreatureLifeStats doubles.
// A created player is offline, so PacketSendUtility sends nothing (Java: player.isOnline() is false).

#include <gtest/gtest.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "ItemsTestSupport.h"
#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/observer/ActionObserver.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/account/PlayerAccountData.h"
#include "aion/gameserver/model/broker/BrokerPlayerCache.h"
#include "aion/gameserver/model/broker/BrokerRace.h"
#include "aion/gameserver/model/drop/Drop.h"
#include "aion/gameserver/model/drop/DropGroup.bind.h"
#include "aion/gameserver/model/drop/DropGroup.h"
#include "aion/gameserver/model/drop/DropItem.h"
#include "aion/gameserver/model/drop/DropModifiers.h"
#include "aion/gameserver/model/enchants/TemperingEffect.h"
#include "aion/gameserver/model/gameobjects/BrokerItem.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/Persistable.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/Equipment.h"
#include "aion/gameserver/model/gameobjects/player/PetCommonData.h"
#include "aion/gameserver/model/gameobjects/player/PetList.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerAppearance.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/items/ChargeInfo.h"
#include "aion/gameserver/model/items/GodStone.h"
#include "aion/gameserver/model/items/IdianStone.h"
#include "aion/gameserver/model/items/ItemSlotInfo.h"
#include "aion/gameserver/model/items/ManaStone.h"
#include "aion/gameserver/model/items/RandomBonusEffect.h"
#include "aion/gameserver/model/items/storage/PlayerStorage.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/items/storage/StorageType.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/calc/functions/StatFunction.h"
#include "aion/gameserver/model/stats/container/CreatureGameStats.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/model/templates/item/actions/PolishAction.bind.h"
#include "aion/gameserver/model/templates/item/actions/PolishAction.h"
#include "aion/gameserver/model/templates/stats/ModifiersTemplate.bind.h"
#include "aion/gameserver/model/trade/Exchange.h"
#include "aion/gameserver/model/trade/ExchangeItem.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/base/YieldPoint.h"
#include "aion/gameserver/runtime/collections/HashSet.h"
#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/sched/Clock.h"
#include "aion/gameserver/runtime/sched/DeterministicExecutor.h"
#include "aion/gameserver/runtime/services/LeakCensus.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"

namespace aion::gameserver::model::items {
namespace {

using gameobjects::Item;
using gameobjects::player::Player;
using runtime::Ptr;
using runtime::Ref;
using PersistentState = gameobjects::Persistable::PersistentState;

#define TEST_SCOPE runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST))

std::vector<Ref<gameobjects::player::PetCommonData>> noPets(Player& /*player*/) {
	return {};
}

/** CreatureGameStats double (the stat calculation belongs to P5-01) */
class TestGameStats final : public stats::container::CreatureGameStats {
public:
	explicit TestGameStats(gameobjects::Creature& owner) : CreatureGameStats(owner) {}
	const templates::stats::StatsTemplate* getStatsTemplate() override { return nullptr; }
	int32_t getBaseAttackSpeed() override { return 0; }
	std::unique_ptr<stats::calc::Stat2> getMovementSpeed() override { return nullptr; }
	std::unique_ptr<stats::calc::Stat2> getAttackRange() override { return nullptr; }
	std::unique_ptr<stats::calc::Stat2> getHpRegenRate() override { return nullptr; }
	std::unique_ptr<stats::calc::Stat2> getMpRegenRate() override { return nullptr; }
};

/** CreatureLifeStats double (PlayerLifeStats reads PlayerGameStats) */
class TestLifeStats final : public stats::container::CreatureLifeStats {
public:
	explicit TestLifeStats(gameobjects::Creature& owner) : CreatureLifeStats(owner, 1000, 500) {}
};

/** The real Player with the stat containers of the doubles above */
class TestPlayer final : public Player {
	AION_MAKE_REF_FRIEND
public:
	TestPlayer(CreateKey key, account::PlayerAccountData& playerAccountData, account::Account& account) : Player(key, playerAccountData, account) {}

protected:
	~TestPlayer() override = default;

	void postConstruct() override {
		try {
			Player::postConstruct();
		} catch (const runtime::UnportedException&) {
			// PlayerGameStats(Player&) is P5-01: everything before it ran
		}
		setGameStats(std::make_unique<TestGameStats>(*this));
		setLifeStats(std::make_unique<TestLifeStats>(*this));
	}
};

class ItemStonesTest : public testing::Test {
protected:
	void SetUp() override {
		utils::ThreadPoolManager::installBackend(nullptr);
		utils::ThreadPoolManager::installBackend(std::make_unique<runtime::DeterministicExecutor>(clock, 13));
		utils::idfactory::IDFactory::getInstance().resetForTests();
		runtime::LeakCensus::getInstance().install();
		gameobjects::player::PetList::setPlayerPetsLoaderForTests(&noPets);
	}

	void TearDown() override {
		gameobjects::player::PetList::setPlayerPetsLoaderForTests(nullptr);
		runtime::Reclaimer::getInstance().drain();
		runtime::LeakCensus::getInstance().uninstall();
		utils::ThreadPoolManager::installBackend(nullptr);
		runtime::Reclaimer::getInstance().drain();
	}

	/** Java PlayerService.getPlayer: account, common data, appearance, account data; then the Player */
	static Ref<TestPlayer> createPlayer(int32_t objectId, Ref<account::Account>& account) {
		account = account::Account::create(1000 + objectId);
		Ref<gameobjects::player::PlayerCommonData> commonData = gameobjects::player::PlayerCommonData::create(objectId);
		commonData->setName("Tester" + std::to_string(objectId));
		commonData->setRace(Race::ELYOS);
		Ref<gameobjects::player::PlayerAppearance> appearance = gameobjects::player::PlayerAppearance::create();
		account->addPlayerAccountData(std::make_unique<account::PlayerAccountData>(*account, *commonData, *appearance));
		return gameobjects::VisibleObject::create<TestPlayer>(*account->getPlayerAccountData(objectId), *account);
	}

	test::StaticDataScope staticData;
	runtime::ManualClock clock{0};
};

// ---- ItemStone, ManaStone, GodStone ----------------------------------------------------------------------------------------------------------

TEST_F(ItemStonesTest, StonePersistentStatesFollowJava) {
	TEST_SCOPE;
	test::registerItem(R"(id="167000001" desc="1400123")");
	try {
		static_cast<void>(ManaStone::create(10, 5, 0, PersistentState::NEW));
		ADD_FAILURE() << "Java: Objects.requireNonNull(getItemTemplate(), ...)";
	} catch (const runtime::NullPointerException& e) {
		EXPECT_STREQ(e.what(), "Invalid item ID: 5");
	}

	Ref<ManaStone> fresh = ManaStone::create(10, 167000001, 0, PersistentState::NEW);
	EXPECT_EQ(fresh->getItemObjId(), 10);
	EXPECT_EQ(fresh->getL10nId(), 1400123);
	fresh->setSlot(3);
	EXPECT_EQ(fresh->getSlot(), 3);
	EXPECT_EQ(fresh->getPersistentState(), PersistentState::NEW) << "UPDATE_REQUIRED keeps NEW";
	fresh->setPersistentState(PersistentState::DELETED);
	EXPECT_EQ(fresh->getPersistentState(), PersistentState::NOACTION) << "a NEW stone is never stored: nothing to delete";

	Ref<ManaStone> stored = ManaStone::create(10, 167000001, 1, PersistentState::UPDATED);
	stored->setSlot(2);
	EXPECT_EQ(stored->getPersistentState(), PersistentState::UPDATE_REQUIRED);
	stored->setPersistentState(PersistentState::DELETED);
	EXPECT_EQ(stored->getPersistentState(), PersistentState::DELETED);
	stored->setPersistentState(PersistentState::UPDATED);
	EXPECT_EQ(stored->getPersistentState(), PersistentState::UPDATED);
}

TEST_F(ItemStonesTest, ManaStoneModifiersAreTheTemplateModifiers) {
	TEST_SCOPE;
	const templates::item::ItemTemplate* attackStone =
		test::registerItem(R"(id="167000002")", R"(<modifiers><add name="PHYSICAL_ATTACK" value="5"/><add name="MAXHP" value="30"/></modifiers>)");
	test::registerItem(R"(id="167000003")");
	Ref<ManaStone> stone = ManaStone::create(11, 167000002, 0, PersistentState::NEW);
	ASSERT_EQ(stone->getModifiers().size(), 2);
	EXPECT_EQ(stone->getFirstModifier(), (*attackStone->getModifiers())[0].get());
	EXPECT_EQ(stone->getModifiers().get(1), (*attackStone->getModifiers())[1].get());
	Ref<ManaStone> plain = ManaStone::create(11, 167000003, 1, PersistentState::NEW);
	EXPECT_EQ(plain->getModifiers().size(), 0);
	EXPECT_EQ(plain->getFirstModifier(), nullptr);
}

TEST_F(ItemStonesTest, GodStoneCountsActivationsAndNeedsAPositiveRate) {
	TEST_SCOPE;
	Ref<account::Account> account;
	Ref<TestPlayer> player = createPlayer(21, account);
	const templates::item::ItemTemplate* sword = test::registerItem(R"(id="100000001")");
	test::registerItem(R"(id="168000034")");
	Ref<Item> item = Item::create(2100, sword);
	Ref<GodStone> stone = GodStone::create(*item, 3, 168000034, nullptr, PersistentState::UPDATED);
	EXPECT_EQ(stone->getItemObjId(), 2100);
	EXPECT_EQ(stone->getSlot(), 0);
	stone->increaseActivatedCount();
	EXPECT_EQ(stone->getActivatedCount(), 4);
	EXPECT_EQ(stone->getPersistentState(), PersistentState::UPDATE_REQUIRED);

	float previousRate = configs::main::CustomConfig::GODSTONE_ACTIVATION_RATE.load();
	configs::main::CustomConfig::GODSTONE_ACTIVATION_RATE.store(0.0f);
	EXPECT_FALSE(stone->tryActivate(true, *player)) << "GODSTONE_ACTIVATION_RATE <= 0 never activates (before the godstone info is read)";
	configs::main::CustomConfig::GODSTONE_ACTIVATION_RATE.store(previousRate);
}

/**
 * Runs a second tryActivate of the same stone at the outer call's cooldown store, after its cooldown read (the window of two concurrent hits
 * of the weapon holder, a skill and an auto attack on different tasks).
 */
struct GodStoneInterleaving {
	static inline GodStone* stone = nullptr;
	static inline gameobjects::Creature* target = nullptr;
	static inline std::thread::id outer{};
	static inline int32_t innerCalls = 0;
	static inline bool innerPassed = false;

	static void yield(const char* site) noexcept {
		const std::string_view name(site);
		if (innerCalls > 0 || std::this_thread::get_id() != outer || (name != "AtomicNumber::set" && name != "AtomicNumber::compareAndSet"))
			return;
		innerCalls++;
		try {
			static_cast<void>(stone->tryActivate(true, *target));
		} catch (const runtime::NullPointerException&) {
			innerPassed = true; // past the cooldown: the null godstone info is read
		} catch (...) {
		}
	}
};

TEST_F(ItemStonesTest, GodStoneCooldownLetsOnlyOneOfTwoConcurrentHitsRoll) {
	TEST_SCOPE;
	Ref<account::Account> account;
	Ref<TestPlayer> player = createPlayer(28, account);
	const templates::item::ItemTemplate* sword = test::registerItem(R"(id="100000012")");
	test::registerItem(R"(id="168000035")");
	Ref<Item> item = Item::create(2900, sword);
	struct Config {
		float rate = configs::main::CustomConfig::GODSTONE_ACTIVATION_RATE.load();
		int32_t cooldown = configs::main::CustomConfig::GODSTONE_EVALUATION_COOLDOWN_MILLIS.load();
		Config() {
			configs::main::CustomConfig::GODSTONE_ACTIVATION_RATE.store(1.0f);
			configs::main::CustomConfig::GODSTONE_EVALUATION_COOLDOWN_MILLIS.store(600000);
		}
		~Config() {
			configs::main::CustomConfig::GODSTONE_ACTIVATION_RATE.store(rate);
			configs::main::CustomConfig::GODSTONE_EVALUATION_COOLDOWN_MILLIS.store(cooldown);
		}
	} config;

	// one hit: past the cooldown (the null godstone info throws), then within the cooldown
	Ref<GodStone> stone = GodStone::create(*item, 0, 168000035, nullptr, PersistentState::NEW);
	EXPECT_THROW(static_cast<void>(stone->tryActivate(true, *player)), runtime::NullPointerException);
	EXPECT_FALSE(stone->tryActivate(false, *player)) << "GODSTONE_EVALUATION_COOLDOWN_MILLIS not elapsed";

#if AION_PCT
	// Deviation (D6): two hits interleaved between Java's get() and set() both pass the cooldown; the compare-and-set lets one of them roll
	Ref<GodStone> fresh = GodStone::create(*item, 0, 168000035, nullptr, PersistentState::NEW);
	GodStoneInterleaving::stone = fresh.get();
	GodStoneInterleaving::target = player.get();
	GodStoneInterleaving::outer = std::this_thread::get_id();
	GodStoneInterleaving::innerCalls = 0;
	GodStoneInterleaving::innerPassed = false;
	static constexpr runtime::pct::PctHooks hooks{&GodStoneInterleaving::yield, nullptr, nullptr};
	bool outerPassed = false;
	{
		struct Install {
			Install() { runtime::pct::installHooks(&hooks); }
			~Install() { runtime::pct::installHooks(nullptr); }
		} install;
		try {
			outerPassed = fresh->tryActivate(true, *player);
		} catch (const runtime::NullPointerException&) {
			outerPassed = true;
		}
	}
	EXPECT_EQ(GodStoneInterleaving::innerCalls, 1);
	EXPECT_TRUE(GodStoneInterleaving::innerPassed) << "the interleaved hit stores the cooldown first";
	EXPECT_FALSE(outerPassed) << "the outer hit sees the new cooldown and does not roll";
	GodStoneInterleaving::stone = nullptr;
	GodStoneInterleaving::target = nullptr;
#endif
}

// ---- RandomBonusEffect, IdianStone -----------------------------------------------------------------------------------------------------------

TEST_F(ItemStonesTest, IdianStoneReadsItsTemplatesAndBurnsPolishCharge) {
	Ref<account::Account> account;
	Ref<TestPlayer> player;
	Ref<Item> weapon;
	{
		TEST_SCOPE;
		player = createPlayer(22, account);
		test::StaticDataRegistry& registry = test::StaticDataRegistry::get();
		const templates::item::ItemTemplate* weaponTemplate =
			test::registerItem(R"(id="100000002")", R"(<idian burn_defend="30000" burn_attack="60000"/>)");
		const templates::item::ItemTemplate* polishTemplate = test::registerItem(R"(id="166500001")", "<actions/>");
		registry.polishActions[polishTemplate->getActions()] = test::bindStatic<templates::item::actions::PolishAction>(R"(<polish set_id="7"/>)");
		registry.randomBonusSets[{templates::item::bonuses::StatBonusType::POLISH, 7}] = {
			test::bindStatic<templates::stats::ModifiersTemplate>(R"(<modifiers><add name="MAXHP" value="10"/></modifiers>)"),
			test::bindStatic<templates::stats::ModifiersTemplate>(R"(<modifiers><add name="MAXHP" value="100"/></modifiers>)")};

		EXPECT_EQ(RandomBonusEffect::create(templates::item::bonuses::StatBonusType::POLISH, 7, 2)->getStatBonusId(), 2);
		// ItemRandomBonusData.getTemplate: bonus.getModifiers().get(statBonusId - 1) of an existing set, null only for a missing set
		EXPECT_THROW(static_cast<void>(RandomBonusEffect::create(templates::item::bonuses::StatBonusType::POLISH, 7, 3)),
			runtime::IndexOutOfBoundsException)
			<< "List.get(2) of a set with two modifiers";
		EXPECT_THROW(static_cast<void>(RandomBonusEffect::create(templates::item::bonuses::StatBonusType::POLISH, 8, 1)), runtime::NullPointerException)
			<< "a missing set: RandomBonusEffect dereferences the null template";
		EXPECT_THROW(static_cast<void>(RandomBonusEffect::create(templates::item::bonuses::StatBonusType::INVENTORY, 7, 1)),
			runtime::NullPointerException)
			<< "sets are per bonus type";

		weapon = Item::create(2200, weaponTemplate);
		runtime::LeakCensus::getInstance().onRemovedFromWorld(*weapon, "Item", 2200);
		weapon->setIdianStone(std::make_unique<IdianStone>(166500001, PersistentState::NEW, *weapon, 2, 400000));
		Ptr<IdianStone> stone = weapon->getIdianStone();
		ASSERT_TRUE(stone);
		EXPECT_EQ(stone->getItemObjId(), 2200);
		EXPECT_EQ(stone->getItemId(), 166500001);
		EXPECT_EQ(stone->getPolishNumber(), 2);
		EXPECT_EQ(stone->getPolishCharge(), 400000);

		// PolishChargeCondition: skill values; the drop below 300k sends SM_INVENTORY_UPDATE_ITEM (the offline player receives nothing)
		stone->decreasePolishCharge(*player, 50000);
		EXPECT_EQ(stone->getPolishCharge(), 350000);
		stone->decreasePolishCharge(*player, 60000);
		EXPECT_EQ(stone->getPolishCharge(), 290000);
		stone->onUnEquip(*player); // no action listener: nothing to end
		EXPECT_EQ(stone->getPersistentState(), PersistentState::NEW);

		// skillValue 0 selects burnAttack (isAttacked is false)
		stone->decreasePolishCharge(*player, 0);
		EXPECT_EQ(stone->getPolishCharge(), 230000) << "burn_attack 60000";

		// onEquip registers the observer only for the main hand
		stone->onEquip(*player, getSlotIdMask(ItemSlot::SUB_HAND));
		EXPECT_FALSE(stone->getActionListener());
		// the observer is stored and added to the ObserveController (P4-11b); RandomBonusEffect.applyEffect then casts the stats to
		// PlayerGameStats, which the test double is not
		EXPECT_THROW(stone->onEquip(*player, getSlotIdMask(ItemSlot::MAIN_HAND)), runtime::ClassCastException);
		Ptr<controllers::observer::ActionObserver> listener = stone->getActionListener();
		ASSERT_TRUE(listener);
		player->getObserveController()->removeObserver(*listener); // the calls below drive the listener directly
		listener->attack(*player, 1234);
		EXPECT_EQ(stone->getPolishCharge(), 230000) << "attack with a skill burns nothing";
		listener->attack(*player, 0);
		EXPECT_EQ(stone->getPolishCharge(), 170000) << "an auto attack burns burn_attack (60000)";
		listener->attacked(*player, 1234);
		EXPECT_EQ(stone->getPolishCharge(), 140000) << "being attacked burns burn_defend (30000), with or without a skill";
		listener->attacked(*player, 0);
		EXPECT_EQ(stone->getPolishCharge(), 110000);
		stone->breakActionListener();
		EXPECT_FALSE(stone->getActionListener());

		// templates the constructor dereferences
		Ref<Item> plain = Item::create(2201, test::registerItem(R"(id="100000003")"));
		EXPECT_THROW(static_cast<void>(std::make_unique<IdianStone>(166500001, PersistentState::NEW, *plain, 2, 1)), runtime::NullPointerException)
			<< "no idian action";
		test::registerItem(R"(id="166500002")");
		EXPECT_THROW(static_cast<void>(std::make_unique<IdianStone>(166500002, PersistentState::NEW, *weapon, 2, 1)), runtime::NullPointerException)
			<< "no polish action";

		// polish charge 0: onUnEquip (no listener left), item update, item.setIdianStone(null), DELETED, then ItemStoneListDAO.storeIdianStones
		// (P4-14): without a DatabaseFactory it logs "Can't save stones" and still marks the stones UPDATED, as Java does. Removing the part
		// retires the stone: the borrow stays valid until the task ends.
		stone->decreasePolishCharge(*player, 500000);
		EXPECT_EQ(stone->getPolishCharge(), 0) << "at least 0";
		EXPECT_FALSE(weapon->getIdianStone());
		EXPECT_EQ(stone->getPersistentState(), PersistentState::UPDATED)
			<< "DELETED turned the NEW stone into NOACTION, and ItemStoneListDAO.store sets UPDATED on every stone it was given";
		stone->decreasePolishCharge(*player, 1);
		EXPECT_EQ(stone->getPolishCharge(), 0) << "no charge left: nothing happens";
	}
	runtime::Reclaimer::getInstance().drain();
	EXPECT_EQ(runtime::LeakCensus::getInstance().trackedCount(), 1u) << "the test still holds the weapon";
	weapon.reset();
	runtime::Reclaimer::getInstance().drain();
	EXPECT_EQ(runtime::LeakCensus::getInstance().trackedCount(), 0u) << "stone and bonus effect hold no reference to the item";
	player.reset();
	account.reset();
}

// ---- ChargeInfo ------------------------------------------------------------------------------------------------------------------------------

TEST_F(ItemStonesTest, ChargeInfoClampsThePointsAndReportsChargeBarSteps) {
	Ref<account::Account> account;
	Ref<TestPlayer> player;
	Ref<Item> item;
	{
		TEST_SCOPE;
		player = createPlayer(23, account);
		const templates::item::ItemTemplate* conditioned =
			test::registerItem(R"(id="110100001")", R"(<improve way="1" level="2" burn_attack="200" burn_defend="300"/>)");
		EXPECT_FALSE(Item::create(2300, test::registerItem(R"(id="110100002")"))->getConditioningInfo()) << "no improvement: no charge info";
		item = Item::create(2301, conditioned);
		runtime::LeakCensus::getInstance().onRemovedFromWorld(*item, "Item", 2301);
		Ptr<ChargeInfo> charge = item->getConditioningInfo();
		ASSERT_TRUE(charge) << "Item.updateChargeInfo: charge level 2";
		EXPECT_EQ(charge->getChargePoints(), 0);

		// a bar step is 50000 points
		EXPECT_TRUE(charge->updateChargePoints(100000));
		EXPECT_EQ(charge->getChargePoints(), 100000);
		EXPECT_FALSE(charge->updateChargePoints(10000)) << "step 2 stays step 2";
		EXPECT_TRUE(charge->updateChargePoints(2000000));
		EXPECT_EQ(charge->getChargePoints(), ChargeInfo::LEVEL2) << "at most LEVEL2";
		EXPECT_EQ(item->getPersistentState(), PersistentState::NEW) << "UPDATE_REQUIRED keeps a NEW item NEW";
		item->setPersistentState(PersistentState::UPDATED);

		// observer callbacks burn the template's points; only auto attacks (skill id 0) count
		charge->attack(*player, 1234);
		EXPECT_EQ(charge->getChargePoints(), ChargeInfo::LEVEL2);
		charge->attack(*player, 0);
		EXPECT_EQ(charge->getChargePoints(), ChargeInfo::LEVEL2 - 200);
		EXPECT_EQ(item->getPersistentState(), PersistentState::UPDATE_REQUIRED);
		charge->attacked(*player, 0);
		EXPECT_EQ(charge->getChargePoints(), ChargeInfo::LEVEL2 - 500) << "no player id set: no item update packet";
		charge->attacked(*player, 99);
		EXPECT_EQ(charge->getChargePoints(), ChargeInfo::LEVEL2 - 500);
		EXPECT_TRUE(charge->updateChargePoints(-5000000));
		EXPECT_EQ(charge->getChargePoints(), 0) << "at least 0";
		charge->setPlayer(nullptr);
	}
	runtime::Reclaimer::getInstance().drain();
	item.reset();
	runtime::Reclaimer::getInstance().drain();
	EXPECT_EQ(runtime::LeakCensus::getInstance().trackedCount(), 0u) << "the conditioning info does not retain its item";
	player.reset();
	account.reset();
}

// ---- TemperingEffect ---------------------------------------------------------------------------------------------------------------------------

TEST_F(ItemStonesTest, TemperingWithoutStatsChangesNothing) {
	TEST_SCOPE;
	Ref<account::Account> account;
	Ref<TestPlayer> player = createPlayer(27, account);
	// an accessory without tempering templates: TemperingData.getTemplates returns null, no functions, a warning and no effect
	Ref<Item> ring = Item::create(2800, test::registerItem(R"(id="122000009" item_group="RING")"));
	ring->setTempering(3);
	enchants::TemperingEffect::apply(*player, *ring);
	EXPECT_FALSE(ring->getTemperingEffect());
	// a level without stats in the templates
	static const detail::TemperingTemplates noLevel3{{1, nullptr}};
	test::StaticDataRegistry::get().temperingTemplates[ring->getItemTemplate()] = &noLevel3;
	enchants::TemperingEffect::apply(*player, *ring);
	EXPECT_FALSE(ring->getTemperingEffect());
	// a plume without tempering_name: Java's getTemperingName().equals(...) throws
	Ref<Item> plume = Item::create(2801, test::registerItem(R"(id="187100001" item_group="PLUME")"));
	EXPECT_THROW(enchants::TemperingEffect::apply(*player, *plume), runtime::NullPointerException);
	EXPECT_FALSE(plume->getTemperingEffect());
}

// ---- PlayerStorage, Exchange, DropGroup with group members, BrokerPlayerCache ----------------------------------------------------------------

TEST_F(ItemStonesTest, PlayerStorageActsForItsOwner) {
	TEST_SCOPE;
	Ref<account::Account> account;
	Ref<TestPlayer> player = createPlayer(24, account);
	storage::Storage& inventory = player->getInventory();
	auto& playerInventory = static_cast<storage::PlayerStorage&>(inventory);
	EXPECT_EQ(playerInventory.getActor().get(), static_cast<Player*>(player.get()));
	EXPECT_EQ(inventory.getStorageType(), storage::StorageType::CUBE);

	// Java onLoadHandler: equipped items go to the equipment, the others into the storage
	const templates::item::ItemTemplate* torsoTemplate = test::registerItem(R"(id="110000009")");
	Ref<Item> equipped = Item::create(2400, torsoTemplate, 1, true, getSlotIdMask(ItemSlot::TORSO));
	inventory.onLoadHandler(*equipped);
	EXPECT_FALSE(inventory.getItemByObjId(2400));
	EXPECT_EQ(player->getEquipment().getEquippedItemByObjId(2400).get(), equipped.get());
	Ref<Item> carried = Item::create(2401, torsoTemplate, 1, false, 0);
	inventory.onLoadHandler(*carried);
	EXPECT_EQ(inventory.getItemByObjId(2401).get(), carried.get());

	playerInventory.setOwner(nullptr);
	EXPECT_FALSE(playerInventory.getActor());
	EXPECT_THROW(inventory.onLoadHandler(*equipped), runtime::NullPointerException) << "an equipped item needs the actor's equipment";
	playerInventory.setOwner(*player);
}

TEST_F(ItemStonesTest, ExchangeCollectsItemsKinahAndLooters) {
	TEST_SCOPE;
	Ref<account::Account> firstAccount;
	Ref<account::Account> secondAccount;
	Ref<TestPlayer> first = createPlayer(25, firstAccount);
	Ref<TestPlayer> second = createPlayer(26, secondAccount);
	Ref<trade::Exchange> exchange = trade::Exchange::create(*first, *second);
	EXPECT_EQ(exchange->getActiveplayer().get(), static_cast<Player*>(first.get()));
	EXPECT_EQ(exchange->getTargetPlayer().get(), static_cast<Player*>(second.get()));
	EXPECT_FALSE(exchange->isConfirmed());
	exchange->lock();
	exchange->confirm();
	EXPECT_TRUE(exchange->isLocked() && exchange->isConfirmed());
	exchange->addKinah(1000);
	exchange->addKinah(234);
	EXPECT_EQ(exchange->getKinahCount(), 1234);

	const templates::item::ItemTemplate* potion = test::registerItem(R"(id="160000009" max_stack_count="100")");
	for (int32_t i = 0; i < 18; i++) {
		EXPECT_FALSE(exchange->isExchangeListFull());
		Ref<Item> item = Item::create(2500 + i, potion, 1, false, 0);
		exchange->addItem(2500 + i, *trade::ExchangeItem::create(2500 + i, 1, *item));
	}
	EXPECT_TRUE(exchange->isExchangeListFull()) << "18 items";
	exchange->addItem(2500, *trade::ExchangeItem::create(2500, 5, *Item::create(2600, potion, 5, false, 0)));
	EXPECT_EQ(exchange->getItems().size(), 18) << "a parent item id is mapped once";

	// DropItem looters and DropGroup each-member drops
	test::registerItem(R"(id="188000001")");
	const drop::DropGroup* eachMember =
		test::bindStatic<drop::DropGroup>(R"(<drop_group name="each"><drop item_id="188000001" each_member="true"/></drop_group>)");
	drop::DropModifiers modifiers;
	modifiers.setBoostDropRate(1.0f);
	Ref<runtime::RcHashSet<Ref<drop::DropItem>>> result = runtime::RcHashSet<Ref<drop::DropItem>>::create();
	std::vector<Ptr<Player>> members{Ptr<Player>(*first), Ptr<Player>(*second)};
	EXPECT_EQ(eachMember->tryAddDropItems(*result, 0, modifiers, members), 2) << "one drop item per member";
	for (Ptr<drop::DropItem> dropItem : *result) {
		// (getWinningPlayer of an offline winner looks the player up in the World, which is P4-10)
		EXPECT_TRUE(dropItem->isDistributeItem());
		TestPlayer& member = dropItem->getIndex() == 0 ? *first : *second;
		TestPlayer& otherMember = dropItem->getIndex() == 0 ? *second : *first;
		ASSERT_EQ(dropItem->getPlayerObjIds().size(), 1);
		EXPECT_EQ(dropItem->getPlayerObjIds().get(0), member.getObjectId()) << "members in the order of the collection";
		EXPECT_TRUE(dropItem->isOnlyPossibleLooter(member)) << "the member's object id is the only looter";
		EXPECT_FALSE(dropItem->isOnlyPossibleLooter(otherMember));
		EXPECT_FALSE(dropItem->canViewDropItem(otherMember.getObjectId()));
	}
	result->clear();
	EXPECT_EQ(eachMember->tryAddDropItems(*result, 0, modifiers, {}), 1) << "without group members: one shared drop item";
}

TEST_F(ItemStonesTest, BrokerPlayerCacheRemovesAnItemByIdentity) {
	TEST_SCOPE;
	const templates::item::ItemTemplate* sword = test::registerItem(R"(id="100000010")");
	Ref<Item> firstItem = Item::create(2700, sword);
	Ref<Item> secondItem = Item::create(2701, sword);
	Ref<gameobjects::BrokerItem> first = gameobjects::BrokerItem::create(*firstItem, 100, 1, false, broker::BrokerRace::ELYOS);
	Ref<gameobjects::BrokerItem> second = gameobjects::BrokerItem::create(*secondItem, 200, 1, false, broker::BrokerRace::ELYOS);
	Ref<broker::BrokerPlayerCache> cache = broker::BrokerPlayerCache::create();
	Ptr<runtime::RcArrayList<Ref<gameobjects::BrokerItem>>> original = cache->getBrokerListCache();
	original->add(first);
	original->add(second);
	cache->removeFromCache(*first);
	Ptr<runtime::RcArrayList<Ref<gameobjects::BrokerItem>>> filtered = cache->getBrokerListCache();
	EXPECT_NE(filtered.get(), original.get()) << "Java stores a new list";
	ASSERT_EQ(filtered->size(), 1);
	EXPECT_EQ(filtered->get(0).get(), second.get());
	EXPECT_EQ(original->size(), 2) << "the previous list is unchanged";
}

} // namespace
} // namespace aion::gameserver::model::items
