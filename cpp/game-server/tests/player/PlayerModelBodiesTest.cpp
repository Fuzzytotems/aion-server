// P4-12 bodies of the account model and the player parts that need no Player object: expectations derived by hand from the Java sources
// (model/account/*.java, model/gameobjects/player/*.java and the enum companions of the chunk).

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <limits>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "aion/commons/configuration/ConfigValue.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/configs/main/GSConfig.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/account/AccountTime.h"
#include "aion/gameserver/model/account/CharacterPasskey.h"
#include "aion/gameserver/model/account/Passport.h"
#include "aion/gameserver/model/account/PassportsList.h"
#include "aion/gameserver/model/account/Passport_RewardStatusInfo.h"
#include "aion/gameserver/model/account/PlayerAccountData.h"
#include "aion/gameserver/model/gameobjects/player/AbyssRank.h"
#include "aion/gameserver/model/gameobjects/player/AbyssRank_AbyssRankUpdateTypeInfo.h"
#include "aion/gameserver/model/gameobjects/player/BindPointPosition.h"
#include "aion/gameserver/model/gameobjects/player/BlockList.h"
#include "aion/gameserver/model/gameobjects/player/BlockedPlayer.h"
#include "aion/gameserver/model/gameobjects/player/Cooldowns.h"
#include "aion/gameserver/model/gameobjects/player/CustomPlayerStateInfo.h"
#include "aion/gameserver/model/gameobjects/player/DeniedStatusInfo.h"
#include "aion/gameserver/model/gameobjects/player/FriendList_StatusInfo.h"
#include "aion/gameserver/model/gameobjects/player/HouseOwnerStateInfo.h"
#include "aion/gameserver/model/gameobjects/player/InRoll.h"
#include "aion/gameserver/model/gameobjects/player/Macros.h"
#include "aion/gameserver/model/gameobjects/player/PlayerAppearance.h"
#include "aion/gameserver/model/gameobjects/player/PetCommonData.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/gameobjects/player/PlayerSettings.h"
#include "aion/gameserver/model/gameobjects/player/PortalCooldown.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/gameobjects/player/RecipeList.h"
#include "aion/gameserver/model/gameobjects/player/ReviveTypeInfo.h"
#include "aion/gameserver/model/gameobjects/player/detail/ItemSlotMasks.h"
#include "aion/gameserver/model/gameobjects/player/detail/PlayerMath.h"
#include "aion/gameserver/model/gameobjects/player/npcFaction/ENpcFactionQuestState.h"
#include "aion/gameserver/model/gameobjects/player/npcFaction/NpcFaction.h"
#include "aion/gameserver/model/templates/BoundRadius.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/questEngine/model/QuestStatus.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/utils/stats/AbyssRankEnum.h"

namespace aion::gameserver::model::gameobjects::player {
namespace {

using runtime::Ptr;
using runtime::Ref;
using PersistentState = Persistable::PersistentState;

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

/** Restores GSConfig.TIME_ZONE_ID after a test that sets it */
class TimeZoneScope {
public:
	explicit TimeZoneScope(const char* zoneName) : previous(configs::main::GSConfig::TIME_ZONE_ID.load()) {
		configs::main::GSConfig::TIME_ZONE_ID.store(std::chrono::locate_zone(zoneName));
	}
	~TimeZoneScope() { configs::main::GSConfig::TIME_ZONE_ID.store(previous); }

private:
	const std::chrono::time_zone* previous;
};

Ref<PlayerCommonData> commonData(int32_t objectId, Race race) {
	Ref<PlayerCommonData> data = PlayerCommonData::create(objectId);
	data->setRace(race);
	return data;
}

TEST(PlayerModelBodiesTest, AccountCountsRacesAndReplacesPlayerAccountData) {
	{
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		Ref<account::Account> account = account::Account::create(1);
		Ref<PlayerAppearance> appearance = PlayerAppearance::create();
		EXPECT_TRUE(account->isEmpty());

		Ref<PlayerCommonData> elyos = commonData(10, Race::ELYOS);
		Ref<PlayerCommonData> asmodian = commonData(11, Race::ASMODIANS);
		account->addPlayerAccountData(std::make_unique<account::PlayerAccountData>(*account, *elyos, *appearance));
		account->addPlayerAccountData(std::make_unique<account::PlayerAccountData>(*account, *asmodian, *appearance));
		EXPECT_EQ(account->size(), 2);
		EXPECT_EQ(account->getNumberOf(Race::ELYOS), 1);
		EXPECT_EQ(account->getNumberOf(Race::ASMODIANS), 1);
		EXPECT_EQ(account->getNumberOf(Race::LYCAN), 0);
		EXPECT_FALSE(account->isEmpty());

		// Java: players.put returns the old data of the same character, whose race count is decremented before the new race is counted
		Ref<PlayerCommonData> changedRace = commonData(10, Race::ASMODIANS);
		account->addPlayerAccountData(std::make_unique<account::PlayerAccountData>(*account, *changedRace, *appearance));
		EXPECT_EQ(account->size(), 2);
		EXPECT_EQ(account->getNumberOf(Race::ELYOS), 0);
		EXPECT_EQ(account->getNumberOf(Race::ASMODIANS), 2);
		EXPECT_EQ(account->getPlayerAccountData(10)->getPlayerCommonData().get(), changedRace.get());

		// the level of new common data is 0, so the maximum stays at Java's initial value 1
		EXPECT_EQ(account->getMaxPlayerLevel(), 1);
		EXPECT_EQ(account->getPlayerAccDataList().size(), 2u);

		// range-for and the Java iterator (remove() removes the entry of the last returned value)
		int32_t count = 0;
		for (Ptr<account::PlayerAccountData> data : *account) {
			EXPECT_TRUE(data);
			++count;
		}
		EXPECT_EQ(count, 2);
		auto it = account->iterator();
		ASSERT_TRUE(it.hasNext());
		Ptr<account::PlayerAccountData> first = it.next();
		it.remove();
		EXPECT_EQ(account->size(), 1);
		EXPECT_FALSE(account->getPlayerAccountData(first->getPlayerCommonData()->getPlayerObjId()));

		account->decrementCountOf(Race::ASMODIANS);
		EXPECT_EQ(account->getNumberOf(Race::ASMODIANS), 1);

		account->setName("tester");
		EXPECT_EQ(account->toString(), "Account [id=1, name=tester]");
		Ptr<account::CharacterPasskey> passkey = account->getCharacterPasskey();
		EXPECT_EQ(account->getCharacterPasskey().get(), passkey.get()) << "created once";
		account->increasePassportStamps();
		account->increasePassportStamps();
		EXPECT_EQ(account->getPassportStamps(), 2);
	}
	runtime::Reclaimer::getInstance().drain();
}

TEST(PlayerModelBodiesTest, AccountTimeSplitsHoursAndMinutes) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	Ref<account::AccountTime> time = account::AccountTime::create();
	time->setAccumulatedOnlineTime((60 + 32) * 60 * 1000 + 59999); // 1 h 32 min 59.999 s
	time->setAccumulatedRestTime(3 * 3600 * 1000);
	EXPECT_EQ(time->getAccumulatedOnlineHours(), 1);
	EXPECT_EQ(time->getAccumulatedOnlineMinutes(), 32);
	EXPECT_EQ(time->getAccumulatedRestHours(), 3);
	EXPECT_EQ(time->getAccumulatedRestMinutes(), 0);
}

TEST(PlayerModelBodiesTest, PlayerAccountDataDeletionTimeAndAppearance) {
	{
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		Ref<account::Account> account = account::Account::create(2);
		Ref<PlayerCommonData> data = commonData(20, Race::ELYOS);
		Ref<PlayerAppearance> appearance = PlayerAppearance::create();
		account->addPlayerAccountData(std::make_unique<account::PlayerAccountData>(*account, *data, *appearance));
		Ptr<account::PlayerAccountData> accountData = account->getPlayerAccountData(20);
		EXPECT_EQ(accountData->getDeletionTimeInSeconds(), 0);
		accountData->setDeletionDate(commons::database::Timestamp(std::chrono::milliseconds(5999)));
		EXPECT_EQ(accountData->getDeletionTimeInSeconds(), 5);

		// setAppearance stores the appearance and updates the bounding radius (0.25, 0.25, bound height)
		Ref<PlayerAppearance> taller = PlayerAppearance::create();
		taller->setHeight(1.0f);
		accountData->setAppearance(*taller);
		EXPECT_EQ(accountData->getAppearance().get(), taller.get());
		EXPECT_FLOAT_EQ(data->getBoundRadius()->getUpper(), taller->getBoundHeight());
	}
	runtime::Reclaimer::getInstance().drain();
}

TEST(PlayerModelBodiesTest, PassportRewardStatusPersistenceAndServerDates) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	// 2024-03-01T23:30:00Z
	const commons::database::Timestamp arrive{std::chrono::sys_days(std::chrono::year(2024) / 3 / 1) + std::chrono::hours(23) + std::chrono::minutes(30)};
	Ref<account::Passport> passport = account::Passport::create(7, false, arrive);
	EXPECT_EQ(passport->getRewardStatus(), account::Passport::RewardStatus::AVAILABLE);
	passport->setFakeStamp(true);
	EXPECT_EQ(passport->getRewardStatus(), account::Passport::RewardStatus::UPCOMING);
	passport->setRewarded(true);
	EXPECT_EQ(passport->getRewardStatus(), account::Passport::RewardStatus::TAKEN);
	EXPECT_EQ(getId(account::Passport::RewardStatus::EXPIRED), 3);

	passport->setPersistentState(PersistentState::NEW);
	passport->setPersistentState(PersistentState::UPDATE_REQUIRED);
	EXPECT_EQ(passport->getPersistentState(), PersistentState::UPDATE_REQUIRED);
	passport->setPersistentState(PersistentState::DELETED);
	EXPECT_EQ(passport->getPersistentState(), PersistentState::DELETED);

	Ref<account::PassportsList> list = account::PassportsList::create();
	list->addPassport(*passport);
	EXPECT_TRUE(list->isPassportPresent(7));
	EXPECT_FALSE(list->isPassportPresent(8));
	const auto epochSeconds = static_cast<int32_t>(std::chrono::duration_cast<std::chrono::seconds>(arrive.time_since_epoch()).count());
	EXPECT_EQ(list->getPassport(7, epochSeconds).get(), passport.get());
	EXPECT_FALSE(list->getPassport(7, epochSeconds + 1));
	{
		TimeZoneScope utc("UTC");
		EXPECT_TRUE(list->hasPassportForDay(7, std::chrono::year(2024) / 3 / 1));
		EXPECT_FALSE(list->hasPassportForDay(7, std::chrono::year(2024) / 3 / 2));
	}
	{
		TimeZoneScope tokyo("Asia/Tokyo"); // UTC+9: the arrival is on March 2nd in server time
		EXPECT_FALSE(list->hasPassportForDay(7, std::chrono::year(2024) / 3 / 1));
		EXPECT_TRUE(list->hasPassportForDay(7, std::chrono::year(2024) / 3 / 2));
	}
	list->removePassport(*passport);
	EXPECT_EQ(list->getAllPassports().size(), 0);
}

TEST(PlayerModelBodiesTest, AbyssRankUpdatesStatisticsRanksAndPersistentState) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	TimeZoneScope utc("UTC");
	EXPECT_THROW(static_cast<void>(AbyssRank::create(0, 0, 0, 19, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0)), runtime::IllegalArgumentException);

	// last update in 1970: daily values reset, weekly values move to the last-week values, and the rank raises the max rank
	Ref<AbyssRank> rank = AbyssRank::create(100, 200, 5000, 3, 4, 5, 6, 1, 7, 8, 0, 9, 10, -1, 12);
	EXPECT_EQ(rank->getDailyAP(), 0);
	EXPECT_EQ(rank->getDailyKill(), 0);
	EXPECT_EQ(rank->getDailyGP(), 0);
	EXPECT_EQ(rank->getLastAP(), 200);
	EXPECT_EQ(rank->getLastKill(), 5);
	EXPECT_EQ(rank->getLastGP(), 10);
	EXPECT_EQ(rank->getWeeklyAP(), 0);
	EXPECT_EQ(rank->getWeeklyKill(), 0);
	EXPECT_EQ(rank->getWeeklyGP(), 0);
	EXPECT_EQ(rank->getCurrentGP(), 0) << "negative glory points start at 0";
	EXPECT_EQ(rank->getRank(), utils::stats::AbyssRankEnum::GRADE7_SOLDIER);
	EXPECT_EQ(rank->getMaxRank(), 3);
	EXPECT_GT(rank->getLastUpdate(), 0);
	EXPECT_EQ(rank->getPersistentState(), PersistentState::UPDATE_REQUIRED) << "Java's initial null state accepts UPDATE_REQUIRED";

	// updated today: nothing changes and the state stays as the DAO sets it
	Ref<AbyssRank> current = AbyssRank::create(1, 2, 300, 1, 3, 4, 5, 1, 6, 7, commons::utils::currentTimeMillis(), 8, 9, 10, 11);
	EXPECT_EQ(current->getDailyAP(), 1);
	EXPECT_EQ(current->getWeeklyAP(), 2);
	EXPECT_EQ(current->getPersistentState(), PersistentState::NOACTION);

	// NEW is kept by UPDATE_REQUIRED (the row is inserted later)
	current->setPersistentState(PersistentState::NEW);
	current->addAp(1000);
	EXPECT_EQ(current->getPersistentState(), PersistentState::NEW);
	EXPECT_EQ(current->getAp(), 1300);
	EXPECT_EQ(current->getDailyAP(), 1001);
	EXPECT_EQ(current->getWeeklyAP(), 1002);
	EXPECT_EQ(current->getRank(), utils::stats::AbyssRankEnum::GRADE8_SOLDIER) << "1300 AP reach GRADE8_SOLDIER (1200 AP, no GP)";
	EXPECT_EQ(current->getMaxRank(), 2);
	current->setPersistentState(PersistentState::UPDATED);
	current->addAp(0);
	EXPECT_EQ(current->getPersistentState(), PersistentState::UPDATE_REQUIRED);

	// negative AP never raise the daily/weekly values, the current AP stop at 0
	current->addAp(-5000);
	EXPECT_EQ(current->getAp(), 0);
	EXPECT_EQ(current->getDailyAP(), 1001);

	// the AP cap limits the added points
	{
		AtomicConfigScope<bool> enableApCap(configs::main::CustomConfig::ENABLE_AP_CAP, true);
		AtomicConfigScope<int64_t> apCapValue(configs::main::CustomConfig::AP_CAP_VALUE, 2000);
		current->addAp(5000);
		EXPECT_EQ(current->getAp(), 2000);
	}
	AtomicConfigScope<bool> noApCap(configs::main::CustomConfig::ENABLE_AP_CAP, false);
	current->addAp(150000);
	EXPECT_EQ(current->getRank(), utils::stats::AbyssRankEnum::GRADE1_SOLDIER) << "152000 AP; officer ranks need GP";
	EXPECT_EQ(current->getMaxRank(), 9);

	current->addGp(20, true);
	EXPECT_EQ(current->getCurrentGP(), 30);
	EXPECT_EQ(current->getDailyGP(), 28);
	current->addGp(-100, false);
	EXPECT_EQ(current->getCurrentGP(), 0);
	EXPECT_EQ(current->getDailyGP(), 28);

	current->incrementAllKills();
	EXPECT_EQ(current->getDailyKill(), 4);
	EXPECT_EQ(current->getWeeklyKill(), 5);
	EXPECT_EQ(current->getAllKill(), 6);

	current->setRank(utils::stats::AbyssRankEnum::GENERAL);
	EXPECT_EQ(current->getMaxRank(), 15);
	current->setRank(utils::stats::AbyssRankEnum::GRADE9_SOLDIER);
	EXPECT_EQ(current->getMaxRank(), 15);
}

TEST(PlayerModelBodiesTest, BindPointAndSettingsPersistentStates) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	Ref<BindPointPosition> bindPoint = BindPointPosition::create(1, 0, 0, 0, int8_t{0});
	bindPoint->setPersistentState(PersistentState::UPDATE_REQUIRED);
	EXPECT_EQ(bindPoint->getPersistentState(), PersistentState::NEW);
	bindPoint->setPersistentState(PersistentState::UPDATED);
	bindPoint->setPersistentState(PersistentState::UPDATE_REQUIRED);
	EXPECT_EQ(bindPoint->getPersistentState(), PersistentState::UPDATE_REQUIRED);

	Ref<PlayerSettings> settings = PlayerSettings::create(nullptr, nullptr, nullptr, 0, 0);
	settings->setPersistentState(PersistentState::UPDATED);
	settings->setDeny(getId(DeniedStatus::TRADE) | getId(DeniedStatus::DUEL));
	EXPECT_EQ(settings->getPersistentState(), PersistentState::UPDATE_REQUIRED);
	EXPECT_TRUE(settings->isInDeniedStatus(DeniedStatus::TRADE));
	EXPECT_TRUE(settings->isInDeniedStatus(DeniedStatus::DUEL));
	EXPECT_FALSE(settings->isInDeniedStatus(DeniedStatus::GROUP));
	settings->setPersistentState(PersistentState::UPDATED);
	Ref<runtime::Array<int8_t>> shortcuts = runtime::Array<int8_t>::make(4);
	settings->setShortcuts(shortcuts);
	EXPECT_EQ(settings->getShortcuts().get(), shortcuts.get());
	EXPECT_EQ(settings->getPersistentState(), PersistentState::UPDATE_REQUIRED);
}

TEST(PlayerModelBodiesTest, CooldownsExpireOnReadLikeTheJavaMapOverrides) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	Ref<Cooldowns> cooldowns = Cooldowns::create();
	const int64_t now = commons::utils::currentTimeMillis();
	EXPECT_EQ(cooldowns->put(1, now - 1), std::nullopt) << "a passed reuse time removes (nothing) instead of storing";
	EXPECT_FALSE(cooldowns->hasCooldown(1));
	EXPECT_EQ(cooldowns->put(2, now + 60000), std::nullopt);
	EXPECT_TRUE(cooldowns->hasCooldown(2));
	EXPECT_EQ(cooldowns->get(2), now + 60000);
	int32_t remaining = cooldowns->remainingSeconds(2);
	EXPECT_GE(remaining, 58);
	EXPECT_LE(remaining, 60);
	EXPECT_EQ(cooldowns->put(2, now - 1), now + 60000) << "Java: remove(cooldownId) returns the previous value";
	EXPECT_EQ(cooldowns->remainingSeconds(2), 0);

	// an entry that expired while stored is removed by get (and by hasCooldown, which Java's containsKey implements through get)
	using CooldownMap = runtime::ConcurrentHashMap<int32_t, int64_t>;
	CooldownMap& map = *cooldowns;
	map.put(3, now - 1000);
	EXPECT_TRUE(map.containsKey(3));
	EXPECT_FALSE(cooldowns->hasCooldown(3));
	EXPECT_FALSE(map.containsKey(3));
}

TEST(PlayerModelBodiesTest, BlockListMacrosRecipesAndSmallValueClasses) {
	{
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		Ref<BlockList> blockList = BlockList::create();
		blockList->add(*BlockedPlayer::create(5, "Ben", "rude"));
		blockList->add(*BlockedPlayer::create(6, "Alice", ""));
		EXPECT_EQ(blockList->getSize(), 2);
		EXPECT_TRUE(blockList->contains(5));
		EXPECT_EQ(blockList->getBlockedPlayer("bEN")->getObjId(), 5) << "names compare ignoring case";
		EXPECT_FALSE(blockList->getBlockedPlayer("Bob"));
		EXPECT_EQ(blockList->getBlockedPlayer(6)->getName(), "Alice");
		EXPECT_FALSE(blockList->isFull());
		int32_t iterated = 0;
		for (Ptr<BlockedPlayer> blocked : *blockList) {
			EXPECT_TRUE(blocked);
			++iterated;
		}
		EXPECT_EQ(iterated, 2);
		auto it = blockList->iterator();
		it.next();
		it.remove();
		EXPECT_EQ(blockList->getSize(), 1);
		blockList->remove(5);
		blockList->remove(6);
		EXPECT_EQ(blockList->getSize(), 0);
		for (int32_t i = 0; i < BlockList::MAX_BLOCKS; i++)
			blockList->add(*BlockedPlayer::create(100 + i, "p" + std::to_string(i), ""));
		EXPECT_TRUE(blockList->isFull());

		Ref<Macros> macros = Macros::create();
		EXPECT_THROW(macros->add(0, "x"), runtime::IllegalArgumentException);
		EXPECT_THROW(macros->add(13, "x"), runtime::IllegalArgumentException);
		EXPECT_TRUE(macros->add(1, "<a/>"));
		EXPECT_FALSE(macros->add(1, "<b/>")) << "the id was used before";
		EXPECT_TRUE(macros->add(12, "<c/>"));
		EXPECT_EQ(macros->getAll().size(), 2u);
		EXPECT_TRUE(macros->remove(1));
		EXPECT_FALSE(macros->remove(1));

		Ref<RecipeList> recipes = RecipeList::create({3, 4});
		EXPECT_TRUE(recipes->isRecipePresent(3));
		EXPECT_FALSE(recipes->isRecipePresent(5));
		EXPECT_EQ(recipes->size(), 2);

		Ref<InRoll> inRoll = InRoll::create(1, 2, 3, 4);
		inRoll->setIndexd(9);
		EXPECT_EQ(inRoll->getIndex(), 2) << "Java bug kept: setIndexd stores the item id";

		Ref<PortalCooldown> portalCooldown = PortalCooldown::create(300, 1000, 1);
		portalCooldown->increaseEnterCount();
		portalCooldown->decreaseEnterCount(3);
		EXPECT_EQ(portalCooldown->getEnterCount(), -1);
	}
	runtime::Reclaimer::getInstance().drain();
}

TEST(PlayerModelBodiesTest, QuestStateListKeepsDeletedQuestIds) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	Ref<QuestStateList> list = QuestStateList::create();
	Ref<questEngine::model::QuestState> started = questEngine::model::QuestState::create(1006, questEngine::model::QuestStatus::START);
	Ref<questEngine::model::QuestState> completed = questEngine::model::QuestState::create(2008, questEngine::model::QuestStatus::COMPLETE);
	EXPECT_TRUE(list->addQuest(1006, *started));
	EXPECT_FALSE(list->addQuest(1006, *completed)) << "duplicate quest ids are rejected with a warning";
	EXPECT_TRUE(list->addQuest(2008, *completed));
	EXPECT_TRUE(list->hasQuest(2008));
	EXPECT_EQ(list->getAllQuestState().size(), 2u);
	EXPECT_EQ(list->getAllQuestState()[0]->getQuestId(), 1006) << "TreeMap order";
	EXPECT_EQ(list->getUncompletedQuests().size(), 1u);
	ASSERT_EQ(list->getCompletedQuests().size(), 1u) << "Java: new QuestState(id, COMPLETE) has the complete count 1";
	EXPECT_EQ(list->getCompletedQuests()[0]->getQuestId(), 2008);

	// QuestState.setPersistentState is P5-06: deleteQuest removed the state and recorded its id before that call
	EXPECT_THROW(static_cast<void>(list->deleteQuest(1006)), runtime::UnportedException);
	EXPECT_FALSE(list->hasQuest(1006));
	EXPECT_TRUE(list->getDeletedQuestIds().contains(1006));
	EXPECT_FALSE(list->deleteQuest(1006));
	EXPECT_EQ(list->getQuestState(2008).get(), completed.get());
}

TEST(PlayerModelBodiesTest, EnumCompanionsHoldTheJavaConstructorData) {
	EXPECT_EQ(getMask(CustomPlayerState::WATCHING_CUTSCENE), 1);
	EXPECT_EQ(getMask(CustomPlayerState::NEUTRAL_TO_ALL_PLAYERS), 512);
	EXPECT_EQ(getMask(CustomPlayerState::ENEMY_OF_EVERYONE), 64 | 128);
	EXPECT_EQ(getMask(CustomPlayerState::NEUTRAL_TO_EVERYONE), 256 | 512);
	EXPECT_EQ(getId(DeniedStatus::VIEW_DETAILS), 1);
	EXPECT_EQ(getId(DeniedStatus::FRIEND), 16);
	EXPECT_EQ(getId(HouseOwnerState::BIDDING_ALLOWED), 4);
	EXPECT_EQ(getReviveTypeId(ReviveType::INSTANCE_REVIVE), 6);
	EXPECT_EQ(getReviveTypeById(8), ReviveType::OBELISK_REVIVE);
	EXPECT_THROW(static_cast<void>(getReviveTypeById(5)), runtime::IllegalArgumentException);
	EXPECT_EQ(value(AbyssRank::AbyssRankUpdateType::LEGION_ASMODIANS), 8);
	EXPECT_EQ(getId(FriendList_Status::AWAY), 3);
	EXPECT_EQ(getByValue(int8_t{1}), FriendList_Status::ONLINE);
	EXPECT_EQ(getByValue(int8_t{2}), std::nullopt);

	// ItemSlot data used by Equipment and PlayerAccountData (detail/ItemSlotMasks.h, Java ItemSlot)
	std::vector<items::ItemSlot> slots = detail::getSlotsFor(detail::MAIN_OR_SUB);
	ASSERT_EQ(slots.size(), 2u);
	EXPECT_EQ(slots[0], items::ItemSlot::MAIN_HAND);
	EXPECT_EQ(slots[1], items::ItemSlot::SUB_HAND);
	EXPECT_EQ(detail::getSlotsFor(detail::ALL_STIGMA).size(), 6u);
	EXPECT_THROW(static_cast<void>(detail::getSlotsFor(0)), runtime::IllegalArgumentException);
	EXPECT_EQ(detail::getEquipmentSlotType(detail::RING_LEFT), 0) << "rings are not visible";
	EXPECT_EQ(detail::getEquipmentSlotType(detail::SUB_HAND), 2);
	EXPECT_EQ(detail::getEquipmentSlotType(detail::MAIN_OR_SUB), 1) << "two-handed weapons use the default slot";
	EXPECT_EQ(detail::getEquipmentSlotType(detail::TORSO), 1);
	EXPECT_EQ(detail::STIGMA2, 2147483648LL);
}

TEST(PlayerModelBodiesTest, ConstructorsReadingStaticDataNeedThePublishedHolder) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	// Java: DataManager.NPC_FACTIONS_DATA.getNpcFactionById(id).isMentor() on the null static field: NullPointerException
	EXPECT_THROW(static_cast<void>(npcFaction::NpcFaction::create(1, 0, false, npcFaction::ENpcFactionQuestState::NOTING, 0)),
		runtime::NullPointerException);
	// Java: DataManager.PET_DATA.getPetTemplate(templateId).containsFunction(...) on the null static field
	EXPECT_THROW(static_cast<void>(PetCommonData::create(1, 2, 3, 0)), runtime::NullPointerException);
}

TEST(PlayerModelBodiesTest, JavaRoundingAndCastsMatchTheJdk) {
	// Math.round(double): floor(x + 0.5) without the 0.49999999999999994 bug of JDK 6; NaN 0; saturating
	EXPECT_EQ(detail::javaRound(0.5), 1);
	EXPECT_EQ(detail::javaRound(-0.5), 0);
	EXPECT_EQ(detail::javaRound(2.5), 3);
	EXPECT_EQ(detail::javaRound(-2.5), -2);
	EXPECT_EQ(detail::javaRound(-2.6), -3);
	EXPECT_EQ(detail::javaRound(0.49999999999999994), 0);
	EXPECT_EQ(detail::javaRound(std::nan("")), 0);
	EXPECT_EQ(detail::javaRound(1e19), std::numeric_limits<int64_t>::max());
	EXPECT_EQ(detail::javaRound(-1e19), std::numeric_limits<int64_t>::min());
	EXPECT_EQ(detail::javaRound(std::numeric_limits<double>::infinity()), std::numeric_limits<int64_t>::max());
	EXPECT_EQ(detail::javaRound(4503599627370497.0), 4503599627370497LL) << "odd integers above 2^52 stay exact";
	// Math.round(float)
	EXPECT_EQ(detail::javaRound(0.5f), 1);
	EXPECT_EQ(detail::javaRound(-0.5f), 0);
	EXPECT_EQ(detail::javaRound(2.5f), 3);
	EXPECT_EQ(detail::javaRound(-1.5f), -1);
	EXPECT_EQ(detail::javaRound(0.49999997f), 0);
	EXPECT_EQ(detail::javaRound(std::nanf("")), 0);
	EXPECT_EQ(detail::javaRound(3e9f), std::numeric_limits<int32_t>::max());
	EXPECT_EQ(detail::javaRound(-3e9f), std::numeric_limits<int32_t>::min());
	// (int) double and (long) float
	EXPECT_EQ(detail::toInt(3e9), std::numeric_limits<int32_t>::max());
	EXPECT_EQ(detail::toInt(-3.7), -3);
	EXPECT_EQ(detail::toInt(std::nan("")), 0);
	EXPECT_EQ(detail::toLong(1e19f), std::numeric_limits<int64_t>::max());
	EXPECT_EQ(detail::toLong(-2.9f), -2);
	EXPECT_EQ(detail::toLong(std::nanf("")), 0);

	// XPLossEnum.getExpLoss: Math.round(expNeed / 100 * param), long division first
	EXPECT_EQ(detail::getExpLoss(5, 100000), 0) << "no loss below level 6";
	EXPECT_EQ(detail::getExpLoss(6, 12345), 123) << "LEVEL_6: 123 * 1.0";
	EXPECT_EQ(detail::getExpLoss(31, 3099), 11) << "LEVEL_40: 30 * 0.35 = 10.5, rounded half up";
	EXPECT_EQ(detail::getExpLoss(50, 1050), 3) << "LEVEL_50: 10 * 0.25 = 2.5";
	EXPECT_EQ(detail::getExpLoss(66, 100000), 0) << "no constant above level 65";
}

TEST(PlayerModelBodiesTest, ServerDatesUseTheIsoWeekAndTheServerZone) {
	using namespace std::chrono;
	const time_zone* utc = locate_zone("UTC");
	const auto millisOf = [](sys_time<milliseconds> time) { return time.time_since_epoch().count(); };

	// AbyssRank.doUpdate compares day, month, year and IsoFields.WEEK_OF_WEEK_BASED_YEAR
	detail::ServerDate sunday = detail::serverDateOf(millisOf(sys_days{2024y / December / 29} + 23h + 59min), utc);
	detail::ServerDate monday = detail::serverDateOf(millisOf(sys_days{2024y / December / 30}), utc);
	EXPECT_EQ(sunday.isoWeek, 52);
	EXPECT_EQ(monday.isoWeek, 1) << "2024-12-30 belongs to week 1 of 2025";
	EXPECT_EQ(monday.date.year(), 2024y) << "the calendar year is compared, not the week-based year";
	detail::ServerDate newYearsEve = detail::serverDateOf(millisOf(sys_days{2020y / December / 31}), utc);
	detail::ServerDate newYear = detail::serverDateOf(millisOf(sys_days{2021y / January / 1}), utc);
	EXPECT_EQ(newYearsEve.isoWeek, 53);
	EXPECT_EQ(newYear.isoWeek, 53) << "same ISO week, but another calendar year: doUpdate resets the weekly values";
	EXPECT_NE(newYearsEve.date.year(), newYear.date.year());
	EXPECT_EQ(detail::serverDateOf(millisOf(sys_days{2021y / January / 4}), utc).isoWeek, 1);
	// the server zone decides the local date: 15:00 UTC is already Monday in Tokyo
	detail::ServerDate tokyo = detail::serverDateOf(millisOf(sys_days{2024y / December / 29} + 15h), locate_zone("Asia/Tokyo"));
	EXPECT_EQ(tokyo.date, 2024y / December / 30);
	EXPECT_EQ(tokyo.isoWeek, 1);

	// NpcFactions.getNextTime: 9:00 today before 9:00, otherwise 9:00 tomorrow (local server time), in epoch seconds
	const auto secondsOf = [](sys_seconds time) { return static_cast<int32_t>(time.time_since_epoch().count()); };
	EXPECT_EQ(detail::npcFactionNextTime(millisOf(sys_days{2024y / March / 10} + 8h + 59min + 59s + 999ms), utc),
		secondsOf(sys_days{2024y / March / 10} + 9h));
	EXPECT_EQ(detail::npcFactionNextTime(millisOf(sys_days{2024y / March / 10} + 9h), utc), secondsOf(sys_days{2024y / March / 11} + 9h));
	EXPECT_EQ(detail::npcFactionNextTime(millisOf(sys_days{2024y / March / 10}), locate_zone("Asia/Tokyo")), secondsOf(sys_days{2024y / March / 11}))
		<< "00:00 UTC is 09:00 in Tokyo: the next 09:00 is tomorrow, 00:00 UTC";
}

} // namespace
} // namespace aion::gameserver::model::gameobjects::player
