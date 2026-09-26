// create<Player> and the player parts (P4-12, milestone M5a prerequisite): construction order, part binding and replacement, the logout and
// zombie breakers and the part bodies that need a Player.
//
// Test doubles, each standing in for a body of a later chunk:
// - PlayerPetsDAO.getPlayerPets (P4-14): PetList::setPlayerPetsLoaderForTests installs a loader returning no pets;
// - the stat containers: TestPlayer runs the real Player::postConstruct, which creates the real PlayerGameStats and PlayerLifeStats (P5-01,
//   wave 5a) after the controller owner, the AI, the aggro list and the move controller, and then replaces them with game and life stats
//   doubles (like the Npc prototype's setupStatContainers). Player::getGameStats() still casts to PlayerGameStats, so the doubles are read through
//   Creature. The real containers are tested in tests/stats.

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "aion/commons/configuration/ConfigValue.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/ai/AbstractAI.h"
#include "aion/gameserver/configs/administration/AdminConfig.h"
#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/configs/main/RatesConfig.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/attack/AttackStatus.h"
#include "aion/gameserver/controllers/attack/PlayerAggroList.h"
#include "aion/gameserver/controllers/movement/PlayerMoveController.h"
#include "aion/gameserver/controllers/observer/ActionObserver.h"
#include "aion/gameserver/controllers/observer/ObserverType.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"
#include "aion/gameserver/model/actions/PlayerMode.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/account/PlayerAccountData.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/AbyssRank.h"
#include "aion/gameserver/model/gameobjects/player/Cooldowns.h"
#include "aion/gameserver/model/gameobjects/player/CustomPlayerState.h"
#include "aion/gameserver/model/gameobjects/player/Equipment.h"
#include "aion/gameserver/model/gameobjects/player/Friend.h"
#include "aion/gameserver/model/gameobjects/player/FriendList.h"
#include "aion/gameserver/model/gameobjects/player/InRoll.h"
#include "aion/gameserver/model/gameobjects/player/LogoutBreakers.h"
#include "aion/gameserver/model/gameobjects/player/Mailbox.h"
#include "aion/gameserver/model/gameobjects/player/PetCommonData.h"
#include "aion/gameserver/model/gameobjects/player/PetList.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerAppearance.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/gameobjects/player/PortalCooldown.h"
#include "aion/gameserver/model/gameobjects/player/PortalCooldownList.h"
#include "aion/gameserver/model/gameobjects/player/PrivateStore.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/gameobjects/player/RatesInfo.h"
#include "aion/gameserver/model/gameobjects/player/RequestResponseHandler.h"
#include "aion/gameserver/model/gameobjects/player/ResponseRequester.h"
#include "aion/gameserver/model/gameobjects/player/emotion/Emotion.h"
#include "aion/gameserver/model/gameobjects/player/emotion/EmotionList.h"
#include "aion/gameserver/model/gameobjects/player/motion/Motion.h"
#include "aion/gameserver/model/gameobjects/player/motion/MotionList.h"
#include "aion/gameserver/model/gameobjects/player/detail/ItemSlotMasks.h"
#include "aion/gameserver/model/gameobjects/player/title/TitleList.h"
#include "aion/gameserver/model/gameobjects/state/FlyState.h"
#include "aion/gameserver/model/items/storage/PlayerStorage.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/items/storage/StorageType.h"
#include "aion/gameserver/model/items/storage/StorageTypeInfo.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/CreatureGameStats.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/model/stats/container/PlayerLifeStats.h"
#include "aion/gameserver/model/team/legion/Legion.h"
#include "aion/gameserver/model/templates/flypath/FlightPath.h"
#include "aion/gameserver/model/templates/item/ItemAttackType.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.bind.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/item/enums/ItemGroup.h"
#include "aion/gameserver/model/templates/ride/RideInfo.h"
#include "aion/gameserver/model/trade/TradePSItem.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/sched/Clock.h"
#include "aion/gameserver/runtime/sched/DeterministicExecutor.h"
#include "aion/gameserver/skillengine/model/ChainSkills.h"
#include "aion/gameserver/runtime/services/LeakCensus.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"

namespace aion::gameserver::model::gameobjects::player {
namespace {

using items::storage::StorageType;
using runtime::Ptr;
using runtime::Ref;

/** Sets an atomic configuration field for the scope and restores the previous value (tests share the process-wide configuration) */
template <class T>
class AtomicConfigScope {
public:
	AtomicConfigScope(std::atomic<T>& configValue, T value) : config(configValue), previous(configValue.load()) { config.store(value); }
	~AtomicConfigScope() { config.store(previous); }
	AtomicConfigScope(const AtomicConfigScope&) = delete;
	AtomicConfigScope& operator=(const AtomicConfigScope&) = delete;

private:
	std::atomic<T>& config;
	const T previous;
};

/** Sets a ConfigValue for the scope and restores the previous value */
template <class T>
class ConfigValueScope {
public:
	ConfigValueScope(commons::configuration::ConfigValue<T>& configValue, T value) : config(configValue), previous(configValue.get()) {
		config.set(std::move(value));
	}
	~ConfigValueScope() { config.set(previous ? *previous : T{}); }
	ConfigValueScope(const ConfigValueScope&) = delete;
	ConfigValueScope& operator=(const ConfigValueScope&) = delete;

private:
	commons::configuration::ConfigValue<T>& config;
	const std::shared_ptr<const T> previous;
};

std::atomic<int32_t> petLoaderCalls{0};
std::atomic<int32_t> destroyedPlayers{0};
std::atomic<bool> petLoaderSawAi{false};
std::atomic<bool> petLoaderSawControllerOwner{false};

std::vector<Ref<PetCommonData>> noPets(Player& player) {
	petLoaderCalls.fetch_add(1);
	// Java order: the Creature constructor created the AI before `new PetList(this)`; getController().setOwner(this) follows it
	try {
		petLoaderSawAi = player.getAi().isOwnerBound();
	} catch (const runtime::NullPointerException&) {
		petLoaderSawAi = false;
	}
	petLoaderSawControllerOwner = player.getController().isOwnerBound();
	return {};
}

/** CreatureGameStats double (the stat calculation belongs to P5-01) */
class TestGameStats final : public stats::container::CreatureGameStats {
public:
	explicit TestGameStats(Creature& owner) : CreatureGameStats(owner) {}
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
	explicit TestLifeStats(Creature& owner) : CreatureLifeStats(owner, 1000, 500) {}
};

/** The real Player with the stat containers of the doubles above */
class TestPlayer final : public Player {
	AION_MAKE_REF_FRIEND
public:
	TestPlayer(CreateKey key, account::PlayerAccountData& playerAccountData, account::Account& account) : Player(key, playerAccountData, account) {}

	/** true if Player::postConstruct stopped at an unported body */
	bool postConstructReachedStats = false;
	/** true if Player::postConstruct created the real PlayerGameStats and PlayerLifeStats before the doubles replaced them */
	bool postConstructCreatedRealStats = false;

protected:
	~TestPlayer() override { destroyedPlayers.fetch_add(1); }

	void postConstruct() override {
		try {
			Player::postConstruct();
		} catch (const runtime::UnportedException&) {
			postConstructReachedStats = true;
		}
		postConstructCreatedRealStats = dynamic_cast<stats::container::PlayerGameStats*>(Creature::getGameStats().get()) != nullptr
			&& dynamic_cast<stats::container::PlayerLifeStats*>(Creature::getLifeStats().get()) != nullptr;
		setGameStats(std::make_unique<TestGameStats>(*this));
		setLifeStats(std::make_unique<TestLifeStats>(*this));
	}
};

/** RequestResponseHandler subclass counting the calls (the Java anonymous handlers override the same two methods) */
class CountingHandler final : public RequestResponseHandler {
	AION_MAKE_REF_FRIEND
public:
	static Ref<CountingHandler> create() { return runtime::makeRef<CountingHandler>(); }

	void acceptRequest(Ptr<Creature> requesterValue, Player& responder) override {
		static_cast<void>(requesterValue);
		static_cast<void>(responder);
		accepted++;
	}
	void denyRequest(Ptr<Creature> requesterValue, Player& responder) override {
		static_cast<void>(requesterValue);
		static_cast<void>(responder);
		denied++;
	}

	int32_t accepted = 0;
	int32_t denied = 0;

protected:
	CountingHandler() : RequestResponseHandler(nullptr) {}
	~CountingHandler() override = default;
};

class PlayerCreationTest : public testing::Test {
protected:
	void SetUp() override {
		utils::ThreadPoolManager::installBackend(nullptr);
		utils::ThreadPoolManager::installBackend(std::make_unique<runtime::DeterministicExecutor>(clock, 7));
		utils::idfactory::IDFactory::getInstance().resetForTests();
		runtime::LeakCensus::getInstance().install();
		PetList::setPlayerPetsLoaderForTests(&noPets);
		petLoaderCalls = 0;
		petLoaderSawAi = false;
		petLoaderSawControllerOwner = true;
		destroyedPlayers = 0;
	}

	void TearDown() override {
		PetList::setPlayerPetsLoaderForTests(nullptr);
		runtime::Reclaimer::getInstance().drain();
		runtime::LeakCensus::getInstance().uninstall();
		utils::ThreadPoolManager::installBackend(nullptr);
		runtime::Reclaimer::getInstance().drain();
	}

	/** Java PlayerService.getPlayer: account, common data, appearance, account data and the account warehouse */
	struct Fixture {
		Ref<account::Account> account;
		Ref<PlayerCommonData> commonData;
		Ptr<account::PlayerAccountData> accountData;
	};

	static Fixture makeAccount(int32_t objectId) {
		Fixture f;
		f.account = account::Account::create(1000 + objectId);
		f.commonData = PlayerCommonData::create(objectId);
		f.commonData->setName("Tester");
		f.commonData->setRace(Race::ELYOS);
		Ref<PlayerAppearance> appearance = PlayerAppearance::create();
		f.account->addPlayerAccountData(std::make_unique<account::PlayerAccountData>(*f.account, *f.commonData, *appearance));
		f.account->setAccountWarehouse(std::make_unique<items::storage::PlayerStorage>(*f.account, StorageType::ACCOUNT_WAREHOUSE));
		f.accountData = f.account->getPlayerAccountData(objectId);
		return f;
	}

	runtime::ManualClock clock{0};
};

TEST_F(PlayerCreationTest, WithoutThePetLoaderTheDaoLogsAndCreationContinues) {
	PetList::setPlayerPetsLoaderForTests(nullptr);
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	Fixture f = makeAccount(1);
	// PlayerPetsDAO.getPlayerPets (P4-14) without an initialized DatabaseFactory logs the SQLException and returns no pets, as Java does, so
	// Player::postConstruct binds the controller owner and completes with the real stat containers
	Ref<TestPlayer> player = VisibleObject::create<TestPlayer>(*f.accountData, *f.account);
	EXPECT_FALSE(player->postConstructReachedStats);
	EXPECT_TRUE(player->postConstructCreatedRealStats);
	EXPECT_TRUE(player->getController().isOwnerBound());
	EXPECT_EQ(petLoaderCalls.load(), 0);
}

TEST_F(PlayerCreationTest, CreateRunsTheConstructorAndPostConstructInJavaOrder) {
	{
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		Fixture f = makeAccount(2);
		Ref<TestPlayer> player = VisibleObject::create<TestPlayer>(*f.accountData, *f.account);
		EXPECT_EQ(petLoaderCalls.load(), 1) << "the pets are loaded once, by postConstruct";
		EXPECT_TRUE(petLoaderSawAi.load()) << "Java: the Creature constructor creates the AI before the PetList loads the pets";
		EXPECT_FALSE(petLoaderSawControllerOwner.load()) << "Java: getController().setOwner(this) follows new PetList(this)";
		EXPECT_FALSE(player->postConstructReachedStats);
		EXPECT_TRUE(player->postConstructCreatedRealStats) << "Java: new PlayerGameStats(this), then new PlayerLifeStats(this)";
		EXPECT_EQ(player->getObjectId(), 2);
		EXPECT_EQ(player->getCommonData().get(), f.commonData.get());
		EXPECT_EQ(player->getAccount().get(), f.account.get());
		EXPECT_EQ(player->getAccountData().get(), f.accountData.get());
		const templates::VisibleObjectTemplate* commonDataTemplate = f.commonData.get();
		EXPECT_EQ(player->getObjectTemplate(), commonDataTemplate);

		// postConstruct: late-bound controller owner, AI (Creature), aggro list (Player::createAggroList), move controller
		EXPECT_EQ(&player->getController().getOwner(), static_cast<VisibleObject*>(player.get()));
		EXPECT_TRUE(player->getAi().isOwnerBound());
		EXPECT_NE(dynamic_cast<controllers::attack::PlayerAggroList*>(&player->getAggroList()), nullptr);
		ASSERT_TRUE(player->getMoveController());
		EXPECT_TRUE(player->getMoveController()->isOwnerBound());

		// constructor parts, each bound to the player
		const runtime::RefCounted* owner = player.get();
		EXPECT_EQ(&player->getResponseRequester().partOwner(), owner);
		EXPECT_EQ(&player->getEquipment().partOwner(), owner);
		EXPECT_EQ(&player->getPortalCooldownList().partOwner(), owner);
		EXPECT_EQ(&player->getPetList().partOwner(), owner);
		EXPECT_EQ(player->getInventory().getStorageType(), StorageType::CUBE);
		EXPECT_EQ(player->getWarehouse().getStorageType(), StorageType::REGULAR_WAREHOUSE);
		std::vector<Ptr<items::storage::Storage>> petBags = player->getPetBags();
		ASSERT_EQ(petBags.size(), 12u);
		EXPECT_EQ(petBags.front()->getStorageType(), StorageType::PET_BAG_6);
		EXPECT_EQ(petBags.back()->getStorageType(), StorageType::CASH_PET_BAG_34);
		std::vector<Ptr<items::storage::Storage>> cabinets = player->getCabinets();
		ASSERT_EQ(cabinets.size(), 20u);
		EXPECT_EQ(cabinets.front()->getStorageType(), StorageType::HOUSE_STORAGE_01);
		EXPECT_EQ(cabinets.back()->getStorageType(), StorageType::HOUSE_STORAGE_20);
		ASSERT_TRUE(player->getQuestStateList());
		EXPECT_NE(player->getCraftCooldowns().get(), player->getHouseObjectCooldowns().get());
		EXPECT_EQ(player->getEquipment().getPersistentState(), Persistable::PersistentState::UPDATED);

		// Java new TitleList(): no Java owner until setTitleList, which binds it
		EXPECT_FALSE(player->getTitleList().getOwner());
		player->setTitleList(std::make_unique<title::TitleList>());
		EXPECT_EQ(player->getTitleList().getOwner().get(), static_cast<Player*>(player.get()));
		EXPECT_EQ(&player->getTitleList().partOwner(), owner);

		// getStorage by StorageType id
		EXPECT_EQ(player->getStorage(getId(StorageType::CUBE)).get(), &player->getInventory());
		EXPECT_EQ(player->getStorage(getId(StorageType::REGULAR_WAREHOUSE)).get(), &player->getWarehouse());
		EXPECT_EQ(player->getStorage(getId(StorageType::ACCOUNT_WAREHOUSE)).get(), &f.account->getAccountWarehouse());
		EXPECT_FALSE(player->getStorage(getId(StorageType::LEGION_WAREHOUSE))) << "no legion";
		EXPECT_EQ(player->getStorage(items::storage::PET_BAG_MIN + 3).get(), petBags[3].get());
		EXPECT_EQ(player->getStorage(items::storage::HOUSE_WH_MAX).get(), cabinets.back().get());
		EXPECT_FALSE(player->getStorage(44));
		EXPECT_FALSE(player->getStorage(getId(StorageType::MAILBOX)));

		// the storages' actor is the player itself: no reference count on the player
		EXPECT_EQ(player->refCount(), 1u);
	}
	runtime::Reclaimer::getInstance().drain();
	EXPECT_EQ(destroyedPlayers.load(), 1) << "no cycle keeps a created player alive";
}

TEST_F(PlayerCreationTest, ReplaceableParts) {
	{
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		Fixture f = makeAccount(3);
		Ref<TestPlayer> player = VisibleObject::create<TestPlayer>(*f.accountData, *f.account);

		// PrivateStore: replaced on every store opening and retired to the Reclaimer (a borrow of the old store stays valid in this task)
		EXPECT_FALSE(player->hasStore());
		player->setStore(std::make_unique<PrivateStore>(*player));
		Ptr<PrivateStore> first = player->getStore();
		ASSERT_TRUE(first);
		player->setStore(std::make_unique<PrivateStore>(*player));
		EXPECT_NE(player->getStore().get(), first.get());
		EXPECT_EQ(&first->getOwner(), static_cast<Player*>(player.get()));
		Ref<trade::TradePSItem> tradeItem = trade::TradePSItem::create(50, 182000001, 2, 100);
		player->getStore()->addItemToSell(50, *tradeItem);
		player->getStore()->addItemToSell(51, *trade::TradePSItem::create(51, 182000002, 1, 10));
		Ptr<runtime::RcLinkedHashMap<int32_t, Ref<trade::TradePSItem>>> soldItems = player->getStore()->getSoldItems();
		player->getStore()->removeItem(50);
		EXPECT_NE(player->getStore()->getSoldItems().get(), soldItems.get()) << "Java replaces the item map";
		EXPECT_FALSE(player->getStore()->getTradeItemByObjId(50));
		EXPECT_TRUE(player->getStore()->getTradeItemByObjId(51));
		EXPECT_EQ(soldItems->size(), 2) << "the old map is unchanged";
		EXPECT_EQ(player->getStore()->getStoreMessage(), "");
		player->setStore(nullptr);
		EXPECT_FALSE(player->hasStore());

		// Mailbox, emotions and motions: null until set
		EXPECT_FALSE(player->getMailbox());
		player->setMailbox(std::make_unique<Mailbox>(*player));
		EXPECT_EQ(player->getMailbox()->size(), 0);
		EXPECT_TRUE(player->getMailbox()->haveFreeSlots());
		EXPECT_FALSE(player->getMailbox()->haveUnread());
		EXPECT_FALSE(player->getEmotions());
		player->setEmotions(std::make_unique<emotion::EmotionList>(*player));
		EXPECT_TRUE(player->getEmotions()->getEmotions().empty());
		player->getEmotions()->add(5, 0, false);
		player->getEmotions()->add(6, 100, false);
		EXPECT_TRUE(player->getEmotions()->contains(5));
		EXPECT_FALSE(player->getEmotions()->contains(7));
		ASSERT_EQ(player->getEmotions()->getEmotions().size(), 2u);
		EXPECT_EQ(player->getEmotions()->getEmotions()[1]->getExpireTime(), 100) << "insertion order";

		EXPECT_THROW(static_cast<void>(player->getMotions()), runtime::NullPointerException) << "Java null before setMotions";
		player->setMotions(std::make_unique<motion::MotionList>(*player));
		EXPECT_FALSE(player->getMotions().getMotions());
		// Java setActive(0, type) without active motions: SM_MOTION(0, type) to the owner, the broadcast of a null map never reaches anybody
		EXPECT_NO_THROW(player->getMotions().setActive(0, 1));
		EXPECT_FALSE(player->getMotions().getActiveMotions());
		player->getMotions().add(*motion::Motion::create(26, 0, true), false);
		player->getMotions().add(*motion::Motion::create(9, 0, false), false);
		EXPECT_EQ(player->getMotions().getMotions()->size(), 2);
		ASSERT_TRUE(player->getMotions().getActiveMotions());
		EXPECT_EQ(player->getMotions().getActiveMotions()->get(3)->getId(), 26) << "motion 26 has motion type 3";

		// FriendList: part created by the DAO and replaced by setFriendList
		Ref<PlayerCommonData> friendData = PlayerCommonData::create(900);
		friendData->setName("Buddy");
		player->setFriendList(std::make_unique<FriendList>(*player, std::vector<Ptr<Friend>>{Friend::create(*friendData, "memo")}));
		FriendList& friends = player->getFriendList();
		EXPECT_EQ(friends.getSize(), 1);
		EXPECT_EQ(friends.getFriend("bUDDY")->getObjectId(), 900);
		EXPECT_EQ(friends.getFriend(900)->getFriendMemo(), "memo");
		EXPECT_EQ(friends.getStatus(), FriendList::Status::OFFLINE);
		EXPECT_EQ(friends.getFriend(900)->getStatus(), FriendList::Status::OFFLINE) << "the friend's common data is offline";
		{
			AtomicConfigScope<int32_t> friendListSize(configs::main::CustomConfig::FRIENDLIST_SIZE, 1);
			EXPECT_TRUE(friends.isFull());
		}
		friends.delFriend(900);
		EXPECT_EQ(friends.getSize(), 0);

		// the two stores retired to the Reclaimer retain their owner until they are destroyed after this task (runtime-architecture.md §2.3)
		EXPECT_EQ(player->refCount(), 3u);
	}
	runtime::Reclaimer::getInstance().drain();
	EXPECT_EQ(destroyedPlayers.load(), 1);
}

TEST_F(PlayerCreationTest, ResponseRequesterHandlesEachRequestOnce) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	Fixture f = makeAccount(4);
	Ref<TestPlayer> player = VisibleObject::create<TestPlayer>(*f.accountData, *f.account);
	ResponseRequester& requester = player->getResponseRequester();

	Ref<CountingHandler> first = CountingHandler::create();
	Ref<CountingHandler> second = CountingHandler::create();
	EXPECT_FALSE(requester.putRequest(1, nullptr));
	EXPECT_TRUE(requester.putRequest(1, first));
	EXPECT_FALSE(requester.putRequest(1, second)) << "putIfAbsent";
	EXPECT_TRUE(requester.putRequest(2, second));
	EXPECT_TRUE(requester.respond(1, 1));
	EXPECT_EQ(first->accepted, 1);
	EXPECT_FALSE(requester.respond(1, 1)) << "the handler was removed";
	EXPECT_TRUE(requester.putRequest(3, first));
	requester.denyAll();
	EXPECT_EQ(first->denied, 1);
	EXPECT_EQ(second->denied, 1);
	EXPECT_FALSE(requester.remove(2));
	EXPECT_TRUE(requester.putRequest(2, second));
	EXPECT_TRUE(requester.remove(2));
	EXPECT_EQ(second->accepted, 0);
}

TEST_F(PlayerCreationTest, PlayerStateBodies) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	Fixture f = makeAccount(5);
	Ref<TestPlayer> player = VisibleObject::create<TestPlayer>(*f.accountData, *f.account);

	EXPECT_EQ(player->getName(), "Tester");
	EXPECT_EQ(player->toString(), "Player [id=5, name=Tester]");
	EXPECT_FALSE(player->isOnline());
	EXPECT_EQ(player->getRace(), Race::ELYOS);
	EXPECT_EQ(player->getOppositeRace(), Race::ASMODIANS);
	EXPECT_EQ(player->getLevel(), 0);

	// custom name tags: String.format(NAME_TAGS[accessLevel - 1], name)
	{
		ConfigValueScope<std::vector<std::string>> nameTags(configs::administration::AdminConfig::NAME_TAGS, {"%s", "<GM>%s"});
		EXPECT_EQ(player->getName(true), "Tester") << "access level 0 has no tag";
		f.account->setAccessLevel(2);
		EXPECT_EQ(player->getName(true), "<GM>Tester");
		EXPECT_EQ(player->getName(false), "Tester");
		f.account->setAccessLevel(3);
		EXPECT_EQ(player->getName(true), "Tester") << "no tag for this level";
	}
	EXPECT_TRUE(player->isStaff());
	EXPECT_TRUE(player->hasAccess(3));
	EXPECT_FALSE(player->hasAccess(4));
	f.account->setMembership(1);
	EXPECT_TRUE(player->hasPermission(1));
	EXPECT_FALSE(player->hasPermission(2));

	// custom and fly states are bit masks
	player->setCustomState(CustomPlayerState::ENEMY_OF_EVERYONE);
	EXPECT_TRUE(player->isInCustomState(CustomPlayerState::ENEMY_OF_ALL_NPCS));
	EXPECT_TRUE(player->isInCustomState(CustomPlayerState::ENEMY_OF_ALL_PLAYERS));
	EXPECT_FALSE(player->isInvulnerable());
	player->unsetCustomState(CustomPlayerState::ENEMY_OF_ALL_PLAYERS);
	EXPECT_FALSE(player->isInCustomState(CustomPlayerState::ENEMY_OF_EVERYONE));
	EXPECT_TRUE(player->isInCustomState(CustomPlayerState::ENEMY_OF_ALL_NPCS));
	player->setCustomState(CustomPlayerState::INVULNERABLE);
	EXPECT_TRUE(player->isInvulnerable());
	EXPECT_FALSE(player->isFlying());
	player->setFlyState(state::FlyState::GLIDING);
	EXPECT_TRUE(player->isFlying());
	EXPECT_TRUE(player->isInGlidingState());
	EXPECT_FALSE(player->isInFlyingState());
	player->unsetFlyState(state::FlyState::GLIDING);
	EXPECT_EQ(player->getFlyState(), 0);

	// prison and gather restriction durations reset themselves once passed
	const int64_t now = commons::utils::currentTimeMillis();
	player->setPrisonEndTimeMillis(now + 90500);
	EXPECT_TRUE(player->isInPrison());
	EXPECT_GE(player->getPrisonDurationSeconds(), 89);
	player->setPrisonEndTimeMillis(now - 5000);
	EXPECT_EQ(player->getPrisonDurationSeconds(), 0);
	EXPECT_FALSE(player->isInPrison());
	player->setGatherRestrictionExpirationTime(now + 10000);
	EXPECT_TRUE(player->isGatherRestricted());
	player->setGatherRestrictionExpirationTime(0);
	EXPECT_FALSE(player->isGatherRestricted());

	EXPECT_EQ(player->getCaptchaWord(), std::nullopt);
	player->setCaptchaWord("abc");
	EXPECT_EQ(player->getCaptchaWord(), std::optional<std::string>("abc"));

	// counter skills are stored under their base status (Java AttackStatus.getBaseStatus), other statuses not at all
	EXPECT_EQ(player->getLastCounterSkill(controllers::attack::AttackStatus::PARRY), 0);
	player->setLastCounterSkill(controllers::attack::AttackStatus::OFFHAND_CRITICAL_PARRY);
	EXPECT_GE(player->getLastCounterSkill(controllers::attack::AttackStatus::PARRY), now);
	player->setLastCounterSkill(controllers::attack::AttackStatus::NORMALHIT);
	EXPECT_EQ(player->getLastCounterSkill(controllers::attack::AttackStatus::NORMALHIT), 0);

	player->setAbyssRankListUpdated(AbyssRank::AbyssRankUpdateType::PLAYER_ASMODIANS);
	EXPECT_TRUE(player->isAbyssRankListUpdated(AbyssRank::AbyssRankUpdateType::PLAYER_ASMODIANS));
	EXPECT_FALSE(player->isAbyssRankListUpdated(AbyssRank::AbyssRankUpdateType::PLAYER_ELYOS));
	player->resetAbyssRankListUpdated();
	EXPECT_FALSE(player->isAbyssRankListUpdated(AbyssRank::AbyssRankUpdateType::PLAYER_ASMODIANS));

	player->addItemCoolDown(7, now + 1000, 1);
	EXPECT_EQ(player->getItemReuseTime(7), now + 1000);
	player->removeItemCoolDown(7);
	EXPECT_EQ(player->getItemReuseTime(7), 0);

	player->setResPosState(true);
	player->setResPosX(1.0f);
	player->unsetResPosState();
	EXPECT_FALSE(player->isInResPostState());
	EXPECT_EQ(player->getResPosX(), 0.0f);

	player->subtractSupplements(3, 0);
	player->updateSupplements(); // Java: nothing to update without a supplement id
	player->setHitTimeBoost(now + 1000, 1.5f);
	EXPECT_TRUE(player->isHitTimeBoosted(now));
	EXPECT_FALSE(player->isHitTimeBoosted(now + 1001));
	EXPECT_EQ(player->getChainSkills().get(), player->getChainSkills().get()) << "created lazily once";
	player->setCurrentFlypath(nullptr);
	EXPECT_EQ(player->getFlyStartTime(), 0);
	player->setFlightTeleportId(42);
	ASSERT_TRUE(player->getFlightPath());
	EXPECT_EQ(player->getFlightPath()->getType(), templates::flypath::FlightPath_Type::FLIGHT_TRANSPORTER);
	EXPECT_FALSE(player->isUsingFlightTransporterOrWindstream()) << "not in the FLYING creature state";
	EXPECT_FALSE(player->isInTeam());
	EXPECT_EQ(player->getCurrentTeamId(), 0);
	EXPECT_FALSE(player->isLegionMember());
	EXPECT_FALSE(player->getLegion());

	// Rates.get: the rate of the account membership, the last one for higher memberships, 1 without rates
	EXPECT_FLOAT_EQ(get(*player, {1.0f, 2.0f, 3.0f}), 2.0f);
	f.account->setMembership(9);
	EXPECT_FLOAT_EQ(get(*player, {1.0f, 2.0f, 3.0f}), 3.0f);
	EXPECT_FLOAT_EQ(get(*player, {}), 1.0f);
	// Rates.X.calcResult: (long) (value * rate) with float arithmetic; the int overload returns the value when the result overflows an int
	{
		ConfigValueScope<std::vector<float>> xpPvpRates(configs::main::RatesConfig::XP_PVP_RATES, {1.0f, 2.5f});
		ConfigValueScope<std::vector<float>> sellLimitRates(configs::main::RatesConfig::SELL_LIMIT_RATES, {3.0f});
		EXPECT_EQ(calcResult(Rates::XP_PVP, *player, int64_t{101}), 252) << "101 * 2.5f = 252.5f, truncated";
		EXPECT_EQ(calcResult(Rates::SELL_LIMIT, *player, int64_t{-7}), -21);
		EXPECT_EQ(calcResult(Rates::SELL_LIMIT, *player, int32_t{1000000000}), 1000000000) << "3e9 does not fit an int: the value is kept";
		EXPECT_EQ(calcResult(Rates::SELL_LIMIT, *player, int32_t{700}), 2100);
	}

	// portal cooldowns: null map until the DAO sets one; an expired cooldown is removed when read
	PortalCooldownList& portalCooldowns = player->getPortalCooldownList();
	EXPECT_FALSE(portalCooldowns.hasCooldowns());
	EXPECT_EQ(portalCooldowns.getPortalCooldownTime(300), 0);
	Ref<runtime::RcHashMap<int32_t, Ref<PortalCooldown>>> map = runtime::RcHashMap<int32_t, Ref<PortalCooldown>>::create();
	map->put(300, PortalCooldown::create(300, now + 60000, 1));
	map->put(301, PortalCooldown::create(301, now - 1, 1));
	portalCooldowns.setPortalCoolDowns(map);
	EXPECT_EQ(portalCooldowns.size(), 2);
	EXPECT_EQ(portalCooldowns.getPortalCooldownTime(300), now + 60000);
	EXPECT_EQ(portalCooldowns.getPortalCooldownTime(301), 0);
	EXPECT_FALSE(portalCooldowns.getPortalCooldown(301));
	EXPECT_FALSE(portalCooldowns.isPortalUseDisabled(301)) << "missing: false before the max entry count is read";
	portalCooldowns.removePortalCooldown(300);
	EXPECT_FALSE(portalCooldowns.hasCooldowns());

	// Equipment without items
	Equipment& equipment = player->getEquipment();
	EXPECT_TRUE(equipment.getEquippedItems().empty());
	EXPECT_FALSE(equipment.getMainHandWeapon());
	EXPECT_FALSE(equipment.isSlotEquipped(1));
	EXPECT_FALSE(equipment.isDualWeaponEquipped());
	EXPECT_FALSE(equipment.isPowerShardEquipped());
	EXPECT_EQ(equipment.getMainHandWeaponType(), std::nullopt);
	EXPECT_EQ(equipment.itemSetPartsEquipped(1), 0);
	EXPECT_EQ(player->getAttackType(), templates::item::ItemAttackType::PHYSICAL);

	// getDirtyItemsToUpdate: only the equipment is UPDATE_REQUIRED here, its state becomes UPDATED
	equipment.setPersistentState(Persistable::PersistentState::UPDATE_REQUIRED);
	EXPECT_TRUE(player->getDirtyItemsToUpdate().empty());
	EXPECT_EQ(equipment.getPersistentState(), Persistable::PersistentState::UPDATED);
}

TEST_F(PlayerCreationTest, LogoutAndZombieBreakersCutTheListedEdges) {
	{
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		Fixture f = makeAccount(6);
		Ref<TestPlayer> player = VisibleObject::create<TestPlayer>(*f.accountData, *f.account);

		// L5: ride observers (an observer list the player owns)
		player->addRideObserver(*controllers::observer::ActionObserver::create(controllers::observer::ObserverType::MOVE));
		player->addRideObserver(*controllers::observer::ActionObserver::create(controllers::observer::ObserverType::MOVE));
		ASSERT_EQ(player->getRideObservers()->size(), 2);
		LogoutBreakers::run(*player); // steps whose helpers of other chunks are unported are skipped, the others still run
		EXPECT_EQ(player->getRideObservers()->size(), 0);
		LogoutBreakers::run(*player); // idempotent

		// the zombie breaker reports only the edges it actually cut
		EXPECT_TRUE(LogoutBreakers::breakZombieEdges(*player).empty());
		player->addRideObserver(*controllers::observer::ActionObserver::create(controllers::observer::ObserverType::MOVE));
		std::vector<const char*> cut = LogoutBreakers::breakZombieEdges(*player);
		ASSERT_EQ(cut.size(), 1u);
		EXPECT_STREQ(cut[0], "rideObservers");
		EXPECT_TRUE(LogoutBreakers::breakZombieEdges(*player).empty());
		// the shared account warehouse's actor is cut and reported while this player is the actor (header request player-3)
		f.account->getAccountWarehouse().setOwner(runtime::Ptr<Player>(*player));
		cut = LogoutBreakers::breakZombieEdges(*player);
		ASSERT_EQ(cut.size(), 1u);
		EXPECT_STREQ(cut[0], "storageActor");
		EXPECT_TRUE(LogoutBreakers::breakZombieEdges(*player).empty());

		// delete breakers: every step runs for a player (D6 runs the logout breakers)
		player->addRideObserver(*controllers::observer::ActionObserver::create(controllers::observer::ObserverType::MOVE));
		LogoutBreakers::onDelete(*player);
		EXPECT_EQ(player->getRideObservers()->size(), 0);
		EXPECT_EQ(player->refCount(), 1u);
	}
	runtime::Reclaimer::getInstance().drain();
	EXPECT_EQ(destroyedPlayers.load(), 1);
}

TEST_F(PlayerCreationTest, EquipmentWithItemsFollowsTheJavaSlotRules) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	Fixture f = makeAccount(10);
	Ref<TestPlayer> player = VisibleObject::create<TestPlayer>(*f.accountData, *f.account);
	Equipment& equipment = player->getEquipment();

	// static data kept for the process, like the DataManager holders keep templates. Weapon templates require weapon skills
	// (PlayerSkillList.isSkillPresent is P5-02), so the items use templates without required skills in weapon slots.
	const auto itemTemplate = [](std::string_view attributes) {
		xml::LoadContext context;
		return xml::bindString<templates::item::ItemTemplate>(context, "<item_template " + std::string(attributes) + "/>").release();
	};
	const templates::item::ItemTemplate* plainTemplate = itemTemplate(R"(id="110000001")");
	const templates::item::ItemTemplate* otherTemplate = itemTemplate(R"(id="110000002")");
	ASSERT_TRUE(plainTemplate->getRequiredSkills().empty());
	ASSERT_FALSE(plainTemplate->isTwoHandWeapon());

	// Java PlayerService loads the equipped items with onLoadHandler: an item in MAIN_OR_SUB takes both hand slots
	Ref<Item> bothHands = Item::create(501, plainTemplate);
	bothHands->setEquipmentSlot(detail::MAIN_OR_SUB);
	Ref<Item> secondSet = Item::create(502, otherTemplate);
	secondSet->setEquipmentSlot(detail::MAIN_OFF_HAND); // the right hand of the second weapon set: not visible
	Ref<Item> torso = Item::create(503, plainTemplate);
	torso->setEquipmentSlot(detail::TORSO);
	Ref<Item> ring = Item::create(504, plainTemplate);
	ring->setEquipmentSlot(detail::RING_LEFT);
	for (const Ref<Item>& item : {bothHands, secondSet, torso, ring})
		equipment.onLoadHandler(*item);

	EXPECT_TRUE(equipment.isSlotEquipped(detail::MAIN_HAND));
	EXPECT_TRUE(equipment.isSlotEquipped(detail::SUB_HAND));
	EXPECT_FALSE(equipment.isSlotEquipped(detail::SUB_OFF_HAND));
	EXPECT_EQ(equipment.getMainHandWeapon().get(), bothHands.get());
	EXPECT_FALSE(equipment.getOffHandWeapon()) << "the sub hand holds the main hand item";
	EXPECT_EQ(equipment.getMainHandWeaponType(), templates::item::enums::ItemGroup::NONE);
	EXPECT_EQ(equipment.getOffHandWeaponType(), std::nullopt);
	EXPECT_FALSE(equipment.isDualWeaponEquipped()) << "no one-handed weapons";

	// TreeMap order by slot mask: MAIN_HAND, SUB_HAND, TORSO, RING_LEFT, MAIN_OFF_HAND
	const auto ids = [](const std::vector<Ptr<Item>>& items) {
		std::vector<int32_t> result;
		for (const Ptr<Item>& item : items)
			result.push_back(item->getObjectId());
		return result;
	};
	EXPECT_EQ(ids(equipment.getEquippedItems()), (std::vector<int32_t>{501, 503, 504, 502})) << "distinct values";
	EXPECT_EQ(ids(equipment.getEquippedItemsWithoutStigma()), (std::vector<int32_t>{501, 501, 503, 504, 502}))
		<< "only two-handed weapons are listed once";
	// appearance: visible slots only (rings and the second weapon set are not); only a two-handed weapon is skipped the second time
	EXPECT_EQ(ids(equipment.getEquippedForAppearance()), (std::vector<int32_t>{501, 501, 503}));
	EXPECT_EQ(equipment.getEquippedItemIds(), (std::unordered_set<int32_t>{110000001, 110000002}));
	EXPECT_EQ(equipment.getEquippedItemByObjId(504).get(), ring.get());
	EXPECT_EQ(equipment.getEquippedItemsByItemId(110000001).size(), 4u) << "Java filters equipment.values(): the MAIN_OR_SUB item twice, torso, ring";
}

TEST_F(PlayerCreationTest, PlayerModesAcceptJavaNull) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	Fixture f = makeAccount(7);
	Ref<TestPlayer> player = VisibleObject::create<TestPlayer>(*f.accountData, *f.account);
	using actions::PlayerMode;

	// Java PlayerActions.setPlayerMode: (RideInfo) obj / (InRoll) obj, where null is a legal cast (PlayerController.onDie passes null)
	static const templates::ride::RideInfo rideInfo;
	player->setPlayerMode(PlayerMode::RIDE, std::any(&rideInfo));
	EXPECT_TRUE(player->isInPlayerMode(PlayerMode::RIDE));
	EXPECT_EQ(player->ride.get(), &rideInfo);
	EXPECT_NO_THROW(player->setPlayerMode(PlayerMode::RIDE, std::any{}));
	EXPECT_FALSE(player->isInPlayerMode(PlayerMode::RIDE));
	player->unsetPlayerMode(PlayerMode::RIDE); // Java: returns false without a ride, no packets

	player->setPlayerMode(PlayerMode::IN_ROLL, std::any(InRoll::create(1, 2, 3, 4)));
	EXPECT_TRUE(player->isInPlayerMode(PlayerMode::IN_ROLL));
	player->unsetPlayerMode(PlayerMode::IN_ROLL);
	EXPECT_FALSE(player->isInPlayerMode(PlayerMode::IN_ROLL));
	player->setPlayerMode(PlayerMode::IN_ROLL, std::any(InRoll::create(1, 2, 3, 4)));
	EXPECT_NO_THROW(player->setPlayerMode(PlayerMode::IN_ROLL, std::any{}));
	EXPECT_FALSE(player->isInPlayerMode(PlayerMode::IN_ROLL));
}

TEST_F(PlayerCreationTest, ZombieBreakerKeepsTheAccountWarehouseActorOfAnotherCharacter) {
	{
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		Fixture f = makeAccount(8);
		// the same account (client connection) enters the world with a second character after the first one leaked
		Ref<PlayerCommonData> secondData = PlayerCommonData::create(9);
		secondData->setName("Second");
		secondData->setRace(Race::ELYOS);
		f.account->addPlayerAccountData(std::make_unique<account::PlayerAccountData>(*f.account, *secondData, *PlayerAppearance::create()));
		Ref<TestPlayer> zombie = VisibleObject::create<TestPlayer>(*f.accountData, *f.account);
		Ref<TestPlayer> second = VisibleObject::create<TestPlayer>(*f.account->getPlayerAccountData(9), *f.account);

		// Java PlayerService.getPlayer: account.getAccountWarehouse().setOwner(player) for the character entering the world
		auto& accountWarehouse = dynamic_cast<items::storage::PlayerStorage&>(f.account->getAccountWarehouse());
		accountWarehouse.setOwner(runtime::Ptr<Player>(*second));
		std::vector<const char*> cut = LogoutBreakers::breakZombieEdges(*zombie);
		for (const char* edge : cut)
			EXPECT_STRNE(edge, "storageActor");
		EXPECT_EQ(accountWarehouse.getActor().get(), static_cast<Player*>(second.get())) << "the other character keeps its account warehouse actor";
		accountWarehouse.setOwner(nullptr);
	}
	runtime::Reclaimer::getInstance().drain();
	EXPECT_EQ(destroyedPlayers.load(), 2);
}

} // namespace
} // namespace aion::gameserver::model::gameobjects::player
