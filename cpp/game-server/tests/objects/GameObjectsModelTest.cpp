// P4-11a bodies of the non-creature game objects: Item (counts, persistent state transitions, masks, sockets, colors, lazily created
// collections), BrokerItem (comparators, persistent state), Letter, HouseDecoration, DropNpc, AssembledNpc, GroupRecruitment and the enum
// companions of model.gameobjects. Expectations are derived by hand from the Java sources named in each test; static data comes from XML text
// bound like the real loader does.

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"
#include "aion/gameserver/model/assemblednpc/AssembledNpc.h"
#include "aion/gameserver/model/assemblednpc/AssembledNpcPart.h"
#include "aion/gameserver/model/broker/BrokerRace.h"
#include "aion/gameserver/model/gameobjects/BrokerItem.h"
#include "aion/gameserver/model/gameobjects/DropNpc.h"
#include "aion/gameserver/model/gameobjects/HouseDecoration.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/Letter.h"
#include "aion/gameserver/model/gameobjects/LetterTypeInfo.h"
#include "aion/gameserver/model/gameobjects/NpcObjectTypeInfo.h"
#include "aion/gameserver/model/gameobjects/PetActionInfo.h"
#include "aion/gameserver/model/gameobjects/PetEmoteInfo.h"
#include "aion/gameserver/model/gameobjects/PetSpecialFunctionInfo.h"
#include "aion/gameserver/model/gameobjects/detail/ObjectsData.h"
#include "aion/gameserver/model/gameobjects/findGroup/GroupRecruitment.h"
#include "aion/gameserver/model/gameobjects/state/CreatureSeeStateInfo.h"
#include "aion/gameserver/model/gameobjects/state/CreatureStateInfo.h"
#include "aion/gameserver/model/gameobjects/state/CreatureVisualStateInfo.h"
#include "aion/gameserver/model/gameobjects/state/FlyStateInfo.h"
#include "aion/gameserver/model/items/storage/ItemStorage.h"
#include "aion/gameserver/model/stats/calc/functions/StatFunction.h"
#include "aion/gameserver/model/templates/assemblednpc/AssembledNpcTemplate_AssembledNpcPartTemplate.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.bind.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/base/YieldPoint.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"

namespace aion::gameserver::model::gameobjects {
namespace {

using runtime::Ptr;
using runtime::Ref;
using PersistentState = Persistable::PersistentState;

// ---- enum companions (Java constructor arguments) ----------------------------------------------------------------------------------------------

static_assert(state::getId(state::CreatureState::ACTIVE) == 1 && state::getId(state::CreatureState::GLIDING) == 512);
static_assert(state::getId(state::CreatureState::CHAIR) == 6 && state::mustMatchExact(state::CreatureState::CHAIR));
static_assert(state::getId(state::CreatureState::DEAD) == 7 && !state::mustMatchExact(state::CreatureState::DEAD));
static_assert(state::getId(state::CreatureState::PRIVATE_SHOP) == 11 && state::mustMatchExact(state::CreatureState::PRIVATE_SHOP));
static_assert(state::getId(state::CreatureState::LOOTING) == 12 && !state::mustMatchExact(state::CreatureState::LOOTING));
static_assert(state::getId(state::CreatureVisualState::BLINKING) == 64 && state::getId(state::CreatureVisualState::HIDE13) == 13);
static_assert(state::getId(state::CreatureSeeState::SEARCH20) == 20 && state::getId(state::FlyState::GLIDING) == 2);
static_assert(getId(NpcObjectType::SERVANT) == 1024 && getId(NpcObjectType::PET) == 2048);
static_assert(getActionId(PetAction::H_ABANDON) == 17 && getActionById(13) == PetAction::SPECIAL_FUNCTION && getActionById(5) == PetAction::UNKNOWN);
static_assert(getEmoteById(150) == PetEmote::LOOT_STOP && getEmoteById(151) == PetEmote::UNKNOWN && getEmoteId(PetEmote::UNKNOWN) == INT32_MAX);
static_assert(getById(3) == PetSpecialFunction::AUTOLOOT && !getById(1).has_value());

TEST(GameObjectEnumCompanionsTest, LetterTypeByIdThrowsJavasMessage) {
	EXPECT_EQ(getLetterTypeById(2), LetterType::BLACKCLOUD);
	try {
		static_cast<void>(getLetterTypeById(3));
		ADD_FAILURE() << "Java throws for an unknown id";
	} catch (const runtime::IllegalArgumentException& e) {
		EXPECT_STREQ(e.what(), "Unsupported revive type: 3");
	}
}

// ---- static data and scope helpers -------------------------------------------------------------------------------------------------------------

std::unique_ptr<templates::item::ItemTemplate> bindItem(std::string_view attributes) {
	xml::LoadContext context;
	return xml::bindString<templates::item::ItemTemplate>(context, "<item_template " + std::string(attributes) + "/>");
}

/** Templates are immortal static data: the tests keep them for the process like the DataManager holders do. */
const templates::item::ItemTemplate* itemTemplate(std::string_view attributes) {
	return bindItem(attributes).release();
}

class GameObjectsModelTest : public testing::Test {
protected:
	void TearDown() override { runtime::Reclaimer::getInstance().drain(); }
};

#define TEST_SCOPE runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST))

// ---- Item ---------------------------------------------------------------------------------------------------------------------------------------

TEST_F(GameObjectsModelTest, ItemConstructorReadsTheTemplate) {
	TEST_SCOPE;
	// Item(objId, itemTemplate), Item.java:78-90
	const templates::item::ItemTemplate* sword =
		itemTemplate(R"(id="100000001" name="Sword" item_group="SWORD" max_stack_count="1" activate_count="3" enchant_type="1" rnd_bonus="5")");
	Ref<Item> item = Item::create(1001, sword);
	EXPECT_EQ(item->getItemCount(), 1);
	EXPECT_EQ(item->getEquipmentSlot(), items::storage::ItemStorage::FIRST_AVAILABLE_SLOT);
	EXPECT_EQ(item->getPersistentState(), PersistentState::NEW);
	EXPECT_EQ(item->getActivationCount(), 3);
	EXPECT_EQ(item->getExpireTime(), 0) << "no template expire time";
	EXPECT_EQ(item->getTuneCount(), -1) << "canTune: rnd_bonus leaves maxTuneCount -1";
	EXPECT_FALSE(item->isIdentified());
	EXPECT_TRUE(item->isAmplified()) << "enchant_type 1";
	EXPECT_FALSE(item->getConditioningInfo()) << "no improvement: no charge info";
	EXPECT_EQ(item->getName(), "Sword");
	EXPECT_EQ(item->getItemId(), 100000001);
	EXPECT_THROW(static_cast<void>(Item::create(1002, nullptr)), runtime::NullPointerException);

	// expire_time in minutes: (now in seconds + minutes * 60) - 1
	const templates::item::ItemTemplate* timed = itemTemplate(R"(id="100000002" expire_time="10")");
	int32_t before = static_cast<int32_t>(commons::utils::currentTimeMillis() / 1000);
	Ref<Item> timedItem = Item::create(1003, timed, 5, true, 7);
	int32_t after = static_cast<int32_t>(commons::utils::currentTimeMillis() / 1000);
	EXPECT_GE(timedItem->getExpireTime(), before + 600 - 1);
	EXPECT_LE(timedItem->getExpireTime(), after + 600 - 1);
	EXPECT_EQ(timedItem->getItemCount(), 5);
	EXPECT_TRUE(timedItem->isEquipped());
	EXPECT_EQ(timedItem->getEquipmentSlot(), 7);
}

TEST_F(GameObjectsModelTest, ItemPersistentStateTransitions) {
	TEST_SCOPE;
	const templates::item::ItemTemplate* potion = itemTemplate(R"(id="160000001" max_stack_count="100")");
	Ref<Item> item = Item::create(2001, potion);
	// Item.setPersistentState, Item.java:518-533: NEW stays NEW on UPDATE_REQUIRED and becomes NOACTION on DELETED
	item->setPersistentState(PersistentState::UPDATE_REQUIRED);
	EXPECT_EQ(item->getPersistentState(), PersistentState::NEW);
	item->setPersistentState(PersistentState::DELETED);
	EXPECT_EQ(item->getPersistentState(), PersistentState::NOACTION);
	item->setPersistentState(PersistentState::UPDATED);
	item->setPersistentState(PersistentState::UPDATE_REQUIRED);
	EXPECT_EQ(item->getPersistentState(), PersistentState::UPDATE_REQUIRED);
	item->setPersistentState(PersistentState::DELETED);
	EXPECT_EQ(item->getPersistentState(), PersistentState::DELETED);

	EXPECT_TRUE(Persistable::DELETED(*item));
	EXPECT_FALSE(Persistable::NEW(*item));
	EXPECT_FALSE(Persistable::CHANGED(*item));
}

TEST_F(GameObjectsModelTest, ItemCountsFollowTheStackCap) {
	TEST_SCOPE;
	const templates::item::ItemTemplate* potion = itemTemplate(R"(id="160000002" max_stack_count="100")");
	Ref<Item> item = Item::create(2002, potion);
	// increaseItemCount, Item.java:335-346: returns what did not fit
	EXPECT_EQ(item->increaseItemCount(0), 0);
	EXPECT_EQ(item->increaseItemCount(50), 0);
	EXPECT_EQ(item->getItemCount(), 51);
	EXPECT_EQ(item->getFreeCount(), 49);
	EXPECT_EQ(item->increaseItemCount(100), 51);
	EXPECT_EQ(item->getItemCount(), 100);
	EXPECT_EQ(item->getPersistentState(), PersistentState::NEW) << "UPDATE_REQUIRED keeps a NEW item NEW";

	// decreaseItemCount, Item.java:354-367: an emptied non-kinah stack is deleted
	item->setPersistentState(PersistentState::UPDATED);
	EXPECT_EQ(item->decreaseItemCount(-1), 0);
	EXPECT_EQ(item->decreaseItemCount(30), 0);
	EXPECT_EQ(item->getItemCount(), 70);
	EXPECT_EQ(item->getPersistentState(), PersistentState::UPDATE_REQUIRED);
	EXPECT_EQ(item->decreaseItemCount(80), 10);
	EXPECT_EQ(item->getItemCount(), 0);
	EXPECT_EQ(item->getPersistentState(), PersistentState::DELETED);

	// kinah stays (UPDATE_REQUIRED) at zero
	const templates::item::ItemTemplate* kinah = itemTemplate(R"(id="182400001" max_stack_count="1")");
	ASSERT_TRUE(kinah->isKinah());
	Ref<Item> money = Item::create(2003, kinah, 10, false, 0);
	money->setPersistentState(PersistentState::UPDATED);
	EXPECT_EQ(money->decreaseItemCount(10), 0);
	EXPECT_EQ(money->getPersistentState(), PersistentState::UPDATE_REQUIRED);
}

TEST_F(GameObjectsModelTest, ItemColorsMasksSocketsAndEnchantParams) {
	TEST_SCOPE;
	// masks: TRADEABLE (1 << 1) | STORABLE_IN_WH (1 << 3) | LEGION_TRADEABLE (1 << 18)
	const templates::item::ItemTemplate* sword =
		itemTemplate(R"(id="100000003" item_group="SWORD" mask="262154" m_slots="4" max_enchant="15")");
	const templates::item::ItemTemplate* shirt = itemTemplate(R"(id="110000001" item_group="CL_TORSO" m_slots="2")");
	const templates::item::ItemTemplate* scroll = itemTemplate(R"(id="169000001")");
	Ref<Item> item = Item::create(3001, sword);

	item->setItemColor(0x12345678);
	EXPECT_EQ(item->getItemColor(), 0x345678) << "no alpha channel";
	item->setItemColor(std::nullopt);
	EXPECT_EQ(item->getItemColor(), std::nullopt);
	EXPECT_EQ(item->getColorTimeLeft(), 0);

	EXPECT_TRUE(item->isTradeable());
	EXPECT_TRUE(item->isLegionTradeable());
	EXPECT_TRUE(item->isStorableInWarehouse());
	EXPECT_FALSE(item->isStorableInAccWarehouse());
	EXPECT_FALSE(item->isSellable());
	item->setSoulBound(true);
	EXPECT_FALSE(item->isTradeable()) << "soul bound";
	EXPECT_TRUE(item->isStorableInWarehouse()) << "the regular warehouse ignores soul binding";

	// getSockets, Item.java:601-616: template slots + optional sockets, at most MAX_BASIC_STONES, only weapons and armor
	item->setOptionalSockets(3);
	EXPECT_EQ(item->getSockets(false), 6);
	EXPECT_EQ(item->getSockets(true), 0) << "no fusioned item";
	item->setFusionedItem(shirt, 0, 1);
	EXPECT_TRUE(item->hasFusionedItem());
	EXPECT_EQ(item->getFusionedItemId(), 110000001);
	EXPECT_EQ(item->getSockets(true), 3);
	EXPECT_EQ(Item::create(3002, scroll)->getSockets(false), 0);

	// getItemEnchantParam, Item.java:917-932
	EXPECT_EQ(item->getMaxEnchantLevel(), 15);
	item->setEnchantLevel(4);
	EXPECT_EQ(item->getItemEnchantParam(), 0);
	item->setEnchantLevel(5);
	EXPECT_EQ(item->getItemEnchantParam(), 1);
	item->setEnchantLevel(15);
	EXPECT_EQ(item->getItemEnchantParam(), 2);
	item->setEnchantLevel(20);
	EXPECT_EQ(item->getItemEnchantParam(), 20);
	Ref<Item> armor = Item::create(3003, shirt);
	armor->setTempering(7);
	EXPECT_EQ(armor->getItemEnchantParam(), 10);
	armor->setTempering(3);
	EXPECT_EQ(armor->getItemEnchantParam(), 3);

	EXPECT_FALSE(item->isSkinnedItem());
	item->setItemSkinTemplate(shirt);
	EXPECT_TRUE(item->isSkinnedItem());
	EXPECT_EQ(item->getItemSkinTemplate(), shirt);
}

TEST_F(GameObjectsModelTest, ItemCollectionsAreCreatedLazilyAndToStringFollowsJava) {
	TEST_SCOPE;
	const templates::item::ItemTemplate* ring = itemTemplate(R"(id="120000001" item_group="RING")");
	Ref<Item> item = Item::create(4001, ring);
	EXPECT_FALSE(item->hasManaStones());
	EXPECT_EQ(item->getItemStonesSize(), 0);
	EXPECT_EQ(item->getFusionStonesSize(), 0);
	EXPECT_EQ(item->toString(),
		"Item [getItemId()=120000001, getObjectId()=4001, itemCount=1, itemColor=null, colorExpireTime=0, itemCreator=null, itemSkinId=120000001, "
		"getFusionedItemId()=0, isEquipped=false, manaStones=null, fusionStones=null, optionalSockets=0, fusionedItemOptionalSockets=0, "
		"getGodStoneId()=0, isSoulBound=false, itemLocation=0, enchantLevel=0, enchantBonus=0, expireTime=0, temporaryExchangeTime=0, "
		"repurchasePrice=0, activationCount=0, bonusNumber=0, tuneCount=0, packCount=0, tempering=0, isAmplified=false, buffSkill=0, "
		"rndPlumeBonusValue=0, getChargePoints()=0]");
	Ptr<runtime::RcTreeSet<Ref<items::ManaStone>>> stones = item->getItemStones();
	ASSERT_TRUE(stones);
	EXPECT_EQ(item->getItemStones(), stones) << "created once";
	EXPECT_NE(item->toString().find("manaStones=[], fusionStones=null"), std::string::npos);

	Ptr<runtime::RcArrayList<Ref<stats::calc::functions::StatFunction>>> modifiers = item->getCurrentModifiers();
	ASSERT_TRUE(modifiers);
	EXPECT_TRUE(modifiers->isEmpty());
	EXPECT_EQ(item->getCurrentModifiers(), modifiers);
}

/**
 * Widens the window between a lazy getter's null check and its publication: while installed, every thread sleeps 2 ms right before it stores a
 * Field<Ref> (exchange, the store of an unguarded `if (!f.get()) f.set(create())`) or compare-and-sets it. The concurrent first callers of a
 * Java-style port then all see null and publish different objects, so the D6 regression tests fail reliably instead of rarely. The yield points
 * exist only in AION_PCT builds (Debug and RelWithDebInfo); elsewhere the tests are smoke tests.
 */
class RaceWindowWidener {
public:
	RaceWindowWidener() { runtime::pct::installHooks(&hooks); }
	~RaceWindowWidener() { runtime::pct::installHooks(nullptr); }
	RaceWindowWidener(const RaceWindowWidener&) = delete;
	RaceWindowWidener& operator=(const RaceWindowWidener&) = delete;

private:
	static void sleepBeforeStore(const char* site) noexcept {
		const std::string_view name(site);
		if (name == "Field<Ref>::exchange" || name == "Field<Ref>::compareAndSet")
			std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}
	static constexpr runtime::pct::PctHooks hooks{&sleepBeforeStore, nullptr, nullptr};
};

/**
 * Deviation (D6, docs/deviations/P4-11a.md): the first calls of the lazy getters on several threads publish one collection. Java's unguarded
 * `if (currentModifiers == null) currentModifiers = new ArrayList<>()` can hand two threads different lists and lose what one of them adds.
 */
TEST_F(GameObjectsModelTest, LazilyCreatedItemCollectionsArePublishedOnce) {
	const templates::item::ItemTemplate* ring = itemTemplate(R"(id="120000002" item_group="RING")");
	for (int round = 0; round < 20; ++round) {
		Ref<Item> item;
		{
			TEST_SCOPE;
			item = Item::create(5000 + round, ring);
		}
		constexpr int threads = 8;
		std::vector<runtime::RcArrayList<Ref<stats::calc::functions::StatFunction>>*> seenModifiers(threads);
		std::vector<runtime::RcTreeSet<Ref<items::ManaStone>>*> seenStones(threads);
		std::atomic<bool> go{false};
		std::vector<std::thread> workers;
		std::optional<RaceWindowWidener> widener(std::in_place);
		for (int t = 0; t < threads; ++t) {
			workers.emplace_back([&, t] {
				while (!go.load())
					std::this_thread::yield();
				TEST_SCOPE;
				seenModifiers[static_cast<size_t>(t)] = item->getCurrentModifiers().get();
				seenStones[static_cast<size_t>(t)] = item->getFusionStones().get();
			});
		}
		go = true;
		for (std::thread& worker : workers)
			worker.join();
		widener.reset();
		TEST_SCOPE;
		for (int t = 0; t < threads; ++t) {
			EXPECT_EQ(seenModifiers[static_cast<size_t>(t)], item->getCurrentModifiers().get());
			EXPECT_EQ(seenStones[static_cast<size_t>(t)], item->getFusionStones().get());
		}
	}
}

// ---- BrokerItem ---------------------------------------------------------------------------------------------------------------------------------

TEST_F(GameObjectsModelTest, BrokerItemCopiesTheItemAndTruncatesTheExpireTime) {
	TEST_SCOPE;
	configs::main::CustomConfig::BROKER_REGISTRATION_EXPIRATION_DAYS = 8;
	const templates::item::ItemTemplate* ore = itemTemplate(R"(id="152000001" name="Ore" level="20" max_stack_count="1000")");
	Ref<Item> item = Item::create(6001, ore, 40, false, 0);
	item->setItemCreator("Smith");
	int64_t before = commons::utils::currentTimeMillis();
	Ref<BrokerItem> brokerItem = BrokerItem::create(*item, 4000, 77, true, broker::BrokerRace::ASMODIAN);
	EXPECT_EQ(brokerItem->getItemId(), 152000001);
	EXPECT_EQ(brokerItem->getItemUniqueId(), 6001);
	EXPECT_EQ(brokerItem->getItemCount(), 40);
	EXPECT_EQ(brokerItem->getItemCreator(), "Smith");
	EXPECT_EQ(brokerItem->getPersistentState(), PersistentState::NEW);
	ASSERT_TRUE(brokerItem->getExpireTime().has_value());
	int64_t expire = brokerItem->getExpireTime()->time_since_epoch().count();
	EXPECT_EQ(expire % 1000, 0) << "Timestamp.setNanos(0)";
	EXPECT_GE(expire, (before + 8LL * 86400000) / 1000 * 1000);
	EXPECT_LE(expire, commons::utils::currentTimeMillis() + 8LL * 86400000);
	configs::main::CustomConfig::BROKER_REGISTRATION_EXPIRATION_DAYS = 0;

	brokerItem->decreaseItemCount(15);
	EXPECT_EQ(brokerItem->getItemCount(), 25);
	EXPECT_EQ(item->getItemCount(), 25);

	// setPersistentState, BrokerItem.java:131-146
	brokerItem->setPersistentState(PersistentState::UPDATE_REQUIRED);
	EXPECT_EQ(brokerItem->getPersistentState(), PersistentState::NEW);
	brokerItem->setPersistentState(PersistentState::DELETED);
	EXPECT_EQ(brokerItem->getPersistentState(), PersistentState::NOACTION);
	brokerItem->setPersistentState(PersistentState::UPDATED);
	brokerItem->setPersistentState(PersistentState::DELETED);
	EXPECT_EQ(brokerItem->getPersistentState(), PersistentState::DELETED);

	// the DAO constructor keeps its state NOACTION and drops the fractional seconds of the stored expire time
	Ref<BrokerItem> loaded = BrokerItem::create(*item, 152000001, 6001, 25, "", 4000, 77, broker::BrokerRace::ELYOS, true, false,
		commons::database::Timestamp(std::chrono::milliseconds(1234567)), commons::database::Timestamp(std::chrono::milliseconds(5)), false);
	EXPECT_EQ(loaded->getPersistentState(), PersistentState::NOACTION);
	EXPECT_EQ(loaded->getExpireTime()->time_since_epoch().count(), 1234000);
	EXPECT_TRUE(loaded->isSold());
	EXPECT_FALSE(loaded->isSettled());
	loaded->setSettled();
	EXPECT_TRUE(loaded->isSettled());
	// BrokerDAO.loadBroker passes a null item for sold entries (header request dao-3)
	Ref<BrokerItem> soldEntry = BrokerItem::create(nullptr, 152000001, 6002, 1, "", 4000, 77, broker::BrokerRace::ELYOS, true, false,
		commons::database::Timestamp(std::chrono::milliseconds(1000)), commons::database::Timestamp(std::chrono::milliseconds(5)), false);
	EXPECT_FALSE(soldEntry->getItem());
	EXPECT_EQ(soldEntry->getItemUniqueId(), 6002);
	EXPECT_THROW(static_cast<void>(BrokerItem::create(*item, 1, 1, 1, "", 1, 1, broker::BrokerRace::ELYOS, false, false, std::nullopt, std::nullopt,
					 false)),
		runtime::NullPointerException)
		<< "Java expireTime.setNanos(0) on null";
}

TEST_F(GameObjectsModelTest, BrokerItemComparators) {
	TEST_SCOPE;
	const templates::item::ItemTemplate* alpha = itemTemplate(R"(id="1" name="Alpha" level="30" max_stack_count="100")");
	const templates::item::ItemTemplate* beta = itemTemplate(R"(id="2" name="beta" level="10" max_stack_count="100")");
	Ref<Item> alphaItem = Item::create(7001, alpha, 10, false, 0);
	Ref<Item> betaItem = Item::create(7002, beta, 2, false, 0);
	Ref<BrokerItem> a = BrokerItem::create(*alphaItem, 1000, 1, true, broker::BrokerRace::ELYOS); // piece price 100
	Ref<BrokerItem> b = BrokerItem::create(*betaItem, 600, 1, true, broker::BrokerRace::ELYOS);   // piece price 300
	Ptr<BrokerItem> none;

	EXPECT_EQ(BrokerItem::getComparatoryByType(0)(a, b), 'A' - 'b') << "String.compareTo: first differing UTF-16 unit";
	EXPECT_EQ(BrokerItem::getComparatoryByType(1)(a, b), 'A' - 'b') << "Java NAME_SORT_DESC compares ascending too";
	EXPECT_EQ(BrokerItem::getComparatoryByType(2)(a, b), 1) << "level 30 > 10";
	EXPECT_EQ(BrokerItem::getComparatoryByType(3)(a, b), -1);
	EXPECT_EQ(BrokerItem::getComparatoryByType(4)(a, b), 1) << "price 1000 > 600";
	EXPECT_EQ(BrokerItem::getComparatoryByType(5)(a, b), -1);
	EXPECT_EQ(BrokerItem::getComparatoryByType(6)(a, b), -1) << "piece price 100 < 300";
	EXPECT_EQ(BrokerItem::getComparatoryByType(7)(a, b), 1);
	EXPECT_EQ(BrokerItem::getComparatoryByType(4)(a, a), 0);
	EXPECT_EQ(BrokerItem::getComparatoryByType(4)(none, a), -1) << "comparePossiblyNull";
	EXPECT_EQ(BrokerItem::getComparatoryByType(4)(a, none), 1);
	EXPECT_EQ(BrokerItem::getComparatoryByType(4)(none, none), 0);
	EXPECT_THROW(static_cast<void>(BrokerItem::getComparatoryByType(8)), runtime::IllegalArgumentException);
	EXPECT_EQ(a->compareTo(*b), -1) << "item unique id 7001 < 7002";
	EXPECT_EQ(b->compareTo(*a), 1);
	EXPECT_EQ(a->compareTo(*a), -1) << "Java never returns 0";

	b->decreaseItemCount(2);
	EXPECT_THROW(static_cast<void>(BrokerItem::getComparatoryByType(6)(a, b)), commons::utils::ArithmeticException) << "Java long division by zero";
}

// ---- Letter, HouseDecoration, DropNpc, AssembledNpc, GroupRecruitment -----------------------------------------------------------------------------

TEST_F(GameObjectsModelTest, LetterExpressFlagAndPersistentState) {
	TEST_SCOPE;
	Ref<Letter> letter = Letter::create(8001, 42, nullptr, 500, "title", "message", "sender",
		commons::database::Timestamp(std::chrono::milliseconds(1000)), true, LetterType::BLACKCLOUD);
	EXPECT_TRUE(letter->isExpress());
	EXPECT_EQ(letter->getPersistentState(), PersistentState::NEW);
	EXPECT_EQ(letter->getName(), "title");
	letter->setLetterType(LetterType::NORMAL);
	EXPECT_FALSE(letter->isExpress());
	EXPECT_EQ(letter->getPersistentState(), PersistentState::NEW) << "Java's setLetterType does not touch the state";
	letter->removeAttachedKinah();
	EXPECT_EQ(letter->getAttachedKinah(), 0);
	EXPECT_EQ(letter->getPersistentState(), PersistentState::UPDATE_REQUIRED);
	letter->setPersistentState(PersistentState::UPDATED);
	letter->setReadLetter();
	EXPECT_FALSE(letter->isUnread());
	EXPECT_EQ(letter->getPersistentState(), PersistentState::UPDATE_REQUIRED);
	EXPECT_EQ(letter->getTimeStamp()->time_since_epoch().count(), 1000);
}

TEST_F(GameObjectsModelTest, HouseDecorationRoomIsAByte) {
	TEST_SCOPE;
	Ref<HouseDecoration> decoration = HouseDecoration::create(9001, 100);
	EXPECT_EQ(decoration->getRoom(), -1);
	EXPECT_EQ(decoration->getPersistentState(), PersistentState::NEW);
	decoration->setRoom(300); // (byte) 300
	EXPECT_EQ(decoration->getRoom(), 44);
}

TEST_F(GameObjectsModelTest, DropNpcFreeForAllClearsTheLooters) {
	TEST_SCOPE;
	Ref<DropNpc> drop = DropNpc::create(9101);
	drop->getAllowedLooters()->add(1);
	drop->setDistributionId(3);
	EXPECT_FALSE(drop->isFreeForAll());
	EXPECT_FALSE(drop->isBeingLooted());
	EXPECT_FALSE(drop->getLootGroupRules()) << "no team: the last rules stay null";
	drop->startFreeForAll();
	EXPECT_TRUE(drop->isFreeForAll());
	EXPECT_EQ(drop->getDistributionId(), 0);
	EXPECT_TRUE(drop->getAllowedLooters()->isEmpty());
}

TEST_F(GameObjectsModelTest, AssembledNpcKeepsItsPartsInOrder) {
	TEST_SCOPE;
	static const auto* first = new templates::assemblednpc::AssembledNpcTemplate_AssembledNpcPartTemplate();
	static const auto* second = new templates::assemblednpc::AssembledNpcTemplate_AssembledNpcPartTemplate();
	Ref<assemblednpc::AssembledNpcPart> part1 = assemblednpc::AssembledNpcPart::create(11, first);
	Ref<assemblednpc::AssembledNpcPart> part2 = assemblednpc::AssembledNpcPart::create(std::nullopt, second);
	EXPECT_EQ(part1->getNpcId(), 0);
	EXPECT_EQ(part2->getStaticId(), 0);
	Ref<assemblednpc::AssembledNpc> npc = assemblednpc::AssembledNpc::create(5, 400010000, 3600, {part1, part2});
	std::vector<Ptr<assemblednpc::AssembledNpcPart>> parts = npc->getAssembledParts().snapshot();
	ASSERT_EQ(parts.size(), 2u);
	EXPECT_EQ(parts[0], part1);
	EXPECT_EQ(parts[1], part2);
	EXPECT_EQ(npc->getRouteId(), 5);
	EXPECT_EQ(npc->getMapId(), 400010000);
	EXPECT_GE(npc->getTimeOnMap(), 0);
}

TEST_F(GameObjectsModelTest, ChunkPrivateDataMatchesTheJavaTables) {
	TEST_SCOPE;
	using summons::UnsummonType;
	// UnsummonType: COMMAND(3000, true) and SKILL_ORDER(3000, false) are delayed, every other constant has delayMillis 0
	for (UnsummonType type : {UnsummonType::LOGOUT, UnsummonType::DISTANCE, UnsummonType::SUMMON_DEATH, UnsummonType::MASTER_DEATH,
			 UnsummonType::UNSPECIFIED, UnsummonType::PET_ORDER_UNSUMMON_EFFECT})
		EXPECT_TRUE(detail::isInstant(type)) << static_cast<int>(type);
	EXPECT_FALSE(detail::isInstant(UnsummonType::COMMAND));
	EXPECT_FALSE(detail::isInstant(UnsummonType::SKILL_ORDER));

	// StaticDoorState: NONE(0), OPENED(1 << 0), CLICKABLE(1 << 1), CLOSEABLE(1 << 2), ONEWAY(1 << 3); setStates skips NONE
	using templates::staticdoor::StaticDoorState;
	EXPECT_EQ(detail::flagOf(StaticDoorState::NONE), 0);
	EXPECT_EQ(detail::flagOf(StaticDoorState::OPENED), 1);
	EXPECT_EQ(detail::flagOf(StaticDoorState::CLICKABLE), 2);
	EXPECT_EQ(detail::flagOf(StaticDoorState::CLOSEABLE), 4);
	EXPECT_EQ(detail::flagOf(StaticDoorState::ONEWAY), 8);
	struct StateSet {
		std::set<StaticDoorState> states;
		void add(StaticDoorState state) { states.insert(state); }
		void remove(StaticDoorState state) { states.erase(state); }
	} doorStates;
	doorStates.add(StaticDoorState::NONE);
	doorStates.add(StaticDoorState::CLICKABLE);
	detail::setStates(1 | 8, doorStates);
	EXPECT_EQ(doorStates.states, (std::set<StaticDoorState>{StaticDoorState::NONE, StaticDoorState::OPENED, StaticDoorState::ONEWAY}))
		<< "flags 9: OPENED and ONEWAY set, CLICKABLE removed, NONE untouched";

	// AbyssRankEnum ids: GRADE9_SOLDIER(1) .. SUPREME_COMMANDER(18)
	EXPECT_EQ(detail::abyssRankId(utils::stats::AbyssRankEnum::GRADE9_SOLDIER), 1);
	EXPECT_EQ(detail::abyssRankId(utils::stats::AbyssRankEnum::STAR1_OFFICER), 10);
	EXPECT_EQ(detail::abyssRankId(utils::stats::AbyssRankEnum::SUPREME_COMMANDER), 18);

	// ItemMask: a template with exactly the bits Item reads, and one with every other bit of the 19 Java constants
	const templates::item::ItemTemplate* allTested = itemTemplate(R"(id="120000100" mask="332862")");
	const templates::item::ItemTemplate* noneTested = itemTemplate(R"(id="120000101" mask="191425")");
	Ref<Item> flagged = Item::create(9301, allTested);
	Ref<Item> other = Item::create(9302, noneTested);
	EXPECT_TRUE(flagged->isTradeable());
	EXPECT_TRUE(flagged->isSellable());
	EXPECT_TRUE(flagged->isStorableInWarehouse());
	EXPECT_TRUE(flagged->isStorableInAccWarehouse());
	EXPECT_TRUE(flagged->isStorableInLegWarehouse());
	EXPECT_TRUE(flagged->canSocketGodstone()) << "CAN_PROC_ENCHANT";
	EXPECT_TRUE(flagged->isRemodelable());
	EXPECT_TRUE(flagged->canApExtract());
	EXPECT_TRUE(flagged->isLegionTradeable());
	EXPECT_FALSE(other->isTradeable());
	EXPECT_FALSE(other->isSellable());
	EXPECT_FALSE(other->isStorableInWarehouse());
	EXPECT_FALSE(other->isStorableInAccWarehouse());
	EXPECT_FALSE(other->isStorableInLegWarehouse());
	EXPECT_FALSE(other->canSocketGodstone());
	EXPECT_FALSE(other->isRemodelable());
	EXPECT_FALSE(other->canApExtract());
	EXPECT_FALSE(other->isLegionTradeable());
	// soul binding keeps the item out of trades and the shared warehouses only
	flagged->setSoulBound(true);
	EXPECT_FALSE(flagged->isTradeable());
	EXPECT_FALSE(flagged->isLegionTradeable());
	EXPECT_FALSE(flagged->isStorableInAccWarehouse());
	EXPECT_FALSE(flagged->isStorableInLegWarehouse());
	EXPECT_TRUE(flagged->isStorableInWarehouse());
	EXPECT_TRUE(flagged->isSellable());

	// house object cooldowns: System.currentTimeMillis() + cd * 1000 with int multiplication
	EXPECT_EQ(detail::cooldownReuseTimeMillis(5000, 60), 65000);
	EXPECT_EQ(detail::cooldownReuseTimeMillis(5000, 3000000), 5000 - 1294967296LL) << "3000000 * 1000 wraps to -1294967296";
}

TEST_F(GameObjectsModelTest, EndOfServerDayResolvesLikeZonedDateTimeWith) {
	using namespace std::chrono;
	const auto millisOf = [](sys_seconds time) { return duration_cast<milliseconds>(time.time_since_epoch()).count(); };
	// ServerTime.now().with(LocalTime.MAX).toEpochSecond() * 1000: 23:59:59 of the local day
	EXPECT_EQ(detail::endOfServerDayMillis(millisOf(sys_days{2024y / March / 10} + 12h), locate_zone("UTC")),
		millisOf(sys_days{2024y / March / 10} + 23h + 59min + 59s));
	EXPECT_EQ(detail::endOfServerDayMillis(millisOf(sys_days{2024y / March / 10} + 16h), locate_zone("Asia/Tokyo")),
		millisOf(sys_days{2024y / March / 11} + 14h + 59min + 59s))
		<< "16:00 UTC is already March 11 in Tokyo (UTC+9)";
	// America/Santiago leaves daylight saving time at 2024-04-07 00:00 (-03) -> 2024-04-06 23:00 (-04): 23:59:59 of April 6 exists twice.
	// ZonedDateTime.with keeps the offset of `now` when it is valid for the new local time.
	const time_zone* santiago = locate_zone("America/Santiago");
	EXPECT_EQ(detail::endOfServerDayMillis(millisOf(sys_days{2024y / April / 6} + 15h), santiago),
		millisOf(sys_days{2024y / April / 7} + 2h + 59min + 59s))
		<< "noon at -03: the first 23:59:59";
	EXPECT_EQ(detail::endOfServerDayMillis(millisOf(sys_days{2024y / April / 7} + 3h + 30min), santiago),
		millisOf(sys_days{2024y / April / 7} + 3h + 59min + 59s))
		<< "23:30 at -04 (second pass): the second 23:59:59, not the one already past";
}

TEST_F(GameObjectsModelTest, GroupRecruitmentOfAnObjectThatIsNeitherPlayerNorTeam) {
	TEST_SCOPE;
	const templates::item::ItemTemplate* dummy = itemTemplate(R"(id="3")");
	Ref<Item> object = Item::create(9201, dummy);
	Ref<findGroup::GroupRecruitment> recruitment = findGroup::GroupRecruitment::create(*object, "lfg", 1);
	EXPECT_EQ(recruitment->getObjectId(), 9201);
	EXPECT_EQ(recruitment->getClassId(), 0);
	EXPECT_EQ(recruitment->getMinLevel(), 1);
	EXPECT_EQ(recruitment->getMaxLevel(), 1);
	EXPECT_EQ(recruitment->getSize(), 1);
	recruitment->setClassId(7);
	recruitment->setLevel(40);
	EXPECT_EQ(recruitment->getClassId(), 7);
	EXPECT_EQ(recruitment->getMinLevel(), 40);
	EXPECT_EQ(recruitment->getMaxLevel(), 1) << "Java ignores the level for the maximum";
	EXPECT_THROW(static_cast<void>(recruitment->getName()), runtime::ClassCastException) << "Java casts the object to Player";
	EXPECT_THROW(static_cast<void>(recruitment->getRace()), runtime::NullPointerException) << "Java returns null for other objects";
	int32_t now = static_cast<int32_t>(commons::utils::currentTimeMillis() / 1000);
	EXPECT_LE(recruitment->getLastUpdate(), now);
	EXPECT_GE(recruitment->getLastUpdate(), now - 5) << "the Java field initializer";
}

} // namespace
} // namespace aion::gameserver::model::gameobjects
