// Round trips of the player DAOs (P4-14) against a fresh aion_gs.sql database: players and common data, abyss ranks and ranking lists (plain
// SQL statements and user variables inside a batch), veteran rewards, advent, cooldowns (batches in transactions), portal cooldowns, bind
// points, emotions, motions, npc factions, titles, appearance, punishments, settings (blobs), skills, friends (two-row batches) and blocks.
//
// Bodies of other chunks that a round trip reaches and that are not ported yet skip the test (SKIP_IF_UNPORTED), so the test starts to run
// once they are ported.

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "DaoTestSupport.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/dao/AbyssRankDAO.h"
#include "aion/gameserver/configs/main/GSConfig.h"
#include "aion/gameserver/controllers/effect/PlayerEffectController.h"
#include "aion/gameserver/dao/AdventDAO.h"
#include "aion/gameserver/dao/BlockListDAO.h"
#include "aion/gameserver/dao/ChallengeTasksDAO.h"
#include "aion/gameserver/dao/PlayerEffectsDAO.h"
#include "aion/gameserver/dao/PlayerQuestListDAO.h"
#include "aion/gameserver/model/challenge/ChallengeTask.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/templates/challenge/ChallengeType.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/questEngine/model/QuestStatus.h"
#include "aion/gameserver/questEngine/model/QuestVars.h"
#include "aion/gameserver/dao/CraftCooldownsDAO.h"
#include "aion/gameserver/dao/FriendListDAO.h"
#include "aion/gameserver/dao/HouseObjectCooldownsDAO.h"
#include "aion/gameserver/dao/ItemCooldownsDAO.h"
#include "aion/gameserver/dao/MotionDAO.h"
#include "aion/gameserver/dao/PlayerAppearanceDAO.h"
#include "aion/gameserver/dao/PlayerBindPointDAO.h"
#include "aion/gameserver/dao/PlayerCooldownsDAO.h"
#include "aion/gameserver/dao/PlayerDAO.h"
#include "aion/gameserver/dao/PlayerEmotionListDAO.h"
#include "aion/gameserver/dao/PlayerNpcFactionsDAO.h"
#include "aion/gameserver/dao/PlayerPunishmentsDAO.h"
#include "aion/gameserver/dao/PlayerSettingsDAO.h"
#include "aion/gameserver/dao/PlayerSkillListDAO.h"
#include "aion/gameserver/dao/PlayerTitleListDAO.h"
#include "aion/gameserver/dao/PortalCooldownsDAO.h"
#include "aion/gameserver/dao/VeteranRewardDAO.h"
#include "aion/gameserver/dao/detail/DaoSupport.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/NpcFactionsData.bind.h"
#include "aion/gameserver/dataholders/PlayerExperienceTable.bind.h"
#include "aion/gameserver/model/Gender.h"
#include "aion/gameserver/model/PlayerClass.h"
#include "aion/gameserver/model/account/CharacterBanInfo.h"
#include "aion/gameserver/model/gameobjects/Persistable.h"
#include "aion/gameserver/model/gameobjects/player/AbyssRank.h"
#include "aion/gameserver/model/gameobjects/player/BindPointPosition.h"
#include "aion/gameserver/model/gameobjects/player/Cooldowns.h"
#include "aion/gameserver/model/gameobjects/player/PlayerSettings.h"
#include "aion/gameserver/model/gameobjects/player/PortalCooldown.h"
#include "aion/gameserver/model/gameobjects/player/PortalCooldownList.h"
#include "aion/gameserver/model/gameobjects/player/emotion/Emotion.h"
#include "aion/gameserver/model/gameobjects/player/emotion/EmotionList.h"
#include "aion/gameserver/model/gameobjects/player/motion/Motion.h"
#include "aion/gameserver/model/gameobjects/player/motion/MotionList.h"
#include "aion/gameserver/model/gameobjects/player/npcFaction/ENpcFactionQuestState.h"
#include "aion/gameserver/model/gameobjects/player/npcFaction/NpcFaction.h"
#include "aion/gameserver/model/gameobjects/player/npcFaction/NpcFactions.h"
#include "aion/gameserver/model/gameobjects/player/title/Title.h"
#include "aion/gameserver/model/items/ItemCooldown.h"
#include "aion/gameserver/model/skill/PlayerSkillEntry.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"
#include "aion/gameserver/model/team/legion/LegionRank.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/collections/HashMap.h"
#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/fields/Array.h"
#include "aion/gameserver/services/PunishmentService_PunishmentType.h"
#include "aion/gameserver/utils/stats/AbyssRankEnum.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::dao::test {
namespace {

using model::gameobjects::Persistable;
using runtime::Ptr;
using runtime::Ref;

class PlayerDaoTest : public DaoTest {
protected:
	static int64_t nowMillis() { return commons::utils::currentTimeMillis(); }
};

TEST_F(PlayerDaoTest, PlayerQueriesById) {
	insertPlayer(10, "Alpha", 100, "ELYOS", 5000);
	insertPlayer(11, "Beta", 100, "ASMODIANS", 99999);
	insertPlayer(12, "Gamma", 101);
	EXPECT_TRUE(PlayerDAO::isNameUsed("Alpha"));
	EXPECT_FALSE(PlayerDAO::isNameUsed("Delta"));
	EXPECT_EQ(PlayerDAO::getPlayerOidsOnAccount(100), (std::vector<int32_t>{10, 11}));
	EXPECT_EQ(PlayerDAO::getPlayerOidsOnAccount(100, 6000), (std::vector<int32_t>{10}));
	EXPECT_EQ(PlayerDAO::getPlayerNameByObjId(11), "Beta");
	EXPECT_FALSE(PlayerDAO::getPlayerNameByObjId(13)) << "Java null";
	EXPECT_EQ(PlayerDAO::getPlayerIdByName("Gamma"), 12);
	EXPECT_EQ(PlayerDAO::getPlayerIdByName("Nobody"), 0);
	EXPECT_EQ(PlayerDAO::getAccountIdByName("Gamma"), 101);
	EXPECT_EQ(PlayerDAO::getAccountId(11), 100);
	EXPECT_EQ(PlayerDAO::getAccountId(99), 0);
	EXPECT_EQ(PlayerDAO::getCharacterCountOnAccount(100), 2);
	PlayerDAO::updateDeletionTime(11, detail::toTimestamp(nowMillis() - 60000));
	EXPECT_EQ(PlayerDAO::getCharacterCountOnAccount(100), 1) << "a past deletion date no longer counts";
	PlayerDAO::updateDeletionTime(11, std::nullopt);
	EXPECT_EQ(PlayerDAO::getCharacterCountOnAccount(100), 2);
	EXPECT_EQ(PlayerDAO::getUsedIDs(), (std::vector<int32_t>{10, 11, 12}));

	PlayerDAO::setPlayerLastTransferTime(10, 1234567890123);
	EXPECT_EQ(queryLong("SELECT last_transfer_time FROM players WHERE id = 10"), 1234567890123);
	PlayerDAO::storeOldCharacterLevel(10, 45);
	EXPECT_EQ(PlayerDAO::getOldCharacterLevel(10), 45);
	EXPECT_EQ(PlayerDAO::getOldCharacterLevel(99), 0);

	execute("UPDATE players SET online = 1");
	EXPECT_TRUE(PlayerDAO::isOnline(10));
	PlayerDAO::setAllPlayersOffline();
	EXPECT_FALSE(PlayerDAO::isOnline(10));
	EXPECT_FALSE(PlayerDAO::isOnline(99));

	PlayerDAO::deletePlayer(12);
	EXPECT_EQ(PlayerDAO::getPlayerIdByName("Gamma"), 0);
}

TEST_F(PlayerDaoTest, CommonDataRoundTrip) {
	insertPlayer(20, "Loaded", 200, "ASMODIANS", 0, "PRIEST");
	execute("UPDATE players SET note = 'a note', title_id = 5, bonus_title_id = 6, quest_expands = 1, npc_expands = 2, item_expands = 3, "
			"wh_npc_expands = 4, wh_bonus_expands = 5, mailbox_letters = 7, dp = 300, soul_sickness = 2, reposte_energy = 99, world_owner = 8, "
			"mentor_flag_time = 9, last_transfer_time = 10, gender = 'FEMALE', online = 1, last_online = '2025-09-15 10:11:12', heading = -3 WHERE id = 20");
	// PlayerCommonData.setExp reads the experience table
	std::string table = "<player_experience_table>";
	for (int32_t level = 0; level <= 80; level++)
		table += "<exp>" + std::to_string(int64_t{level} * 1000) + "</exp>";
	PublishedHolder experience(dataholders::DataManager::PLAYER_EXPERIENCE_TABLE, bindXml<dataholders::PlayerExperienceTable>(table + "</player_experience_table>"));
	Ref<model::gameobjects::player::PlayerCommonData> cd;
	SKIP_IF_UNPORTED(cd = PlayerDAO::loadPlayerCommonData(20));
	ASSERT_TRUE(cd);
	EXPECT_EQ(cd->getName(), "Loaded");
	EXPECT_EQ(cd->getPlayerClass(), model::PlayerClass::PRIEST);
	EXPECT_EQ(cd->getRace(), model::Race::ASMODIANS);
	EXPECT_EQ(cd->getGender(), model::Gender::FEMALE);
	EXPECT_EQ(cd->getNote(), "a note");
	EXPECT_EQ(cd->getTitleId(), 5);
	EXPECT_EQ(cd->getBonusTitleId(), 6);
	EXPECT_EQ(cd->getWhBonusExpands(), 5);
	EXPECT_EQ(cd->getMailboxLetters(), 7);
	EXPECT_EQ(cd->getDeathCount(), 2);
	EXPECT_EQ(cd->getCurrentReposeEnergy(), 99);
	EXPECT_EQ(cd->getWorldOwnerId(), 8);
	EXPECT_EQ(cd->getMentorFlagTime(), 9);
	EXPECT_EQ(cd->getHeading(), -3);
	EXPECT_EQ(cd->getX(), 1.0f);
	EXPECT_EQ(cd->getMapId(), 210010000);
	ASSERT_TRUE(cd->getLastOnline());
	EXPECT_EQ(detail::getTime(*cd->getLastOnline()) / 1000, queryLong("SELECT UNIX_TIMESTAMP(last_online) FROM players WHERE id = 20"));
	EXPECT_FALSE(PlayerDAO::loadPlayerCommonData(21));
	Ref<model::gameobjects::player::PlayerCommonData> byName = PlayerDAO::loadPlayerCommonDataByName("Loaded");
	ASSERT_TRUE(byName);
	EXPECT_EQ(byName->getPlayerObjId(), 20);
	EXPECT_FALSE(PlayerDAO::loadPlayerCommonDataByName("Missing"));

	cd->setName("Renamed");
	PlayerDAO::storePlayerName(*cd);
	EXPECT_EQ(PlayerDAO::getPlayerNameByObjId(20), "Renamed");

	execute("UPDATE players SET creation_date = '2025-01-02 03:04:05', deletion_date = NULL WHERE id = 20");
	Ref<model::gameobjects::player::PlayerAppearance> appearance = model::gameobjects::player::PlayerAppearance::create();
	Ref<model::account::Account> account = model::account::Account::create(200);
	model::account::PlayerAccountData accountData(*account, *cd, *appearance);
	PlayerDAO::setCreationDeletionTime(accountData);
	EXPECT_FALSE(accountData.getDeletionDate());
	ASSERT_TRUE(accountData.getCreationDate());
	EXPECT_EQ(detail::getTime(*accountData.getCreationDate()) / 1000, queryLong("SELECT UNIX_TIMESTAMP(creation_date) FROM players WHERE id = 20"));
}

TEST_F(PlayerDaoTest, NewPlayerIsSavedAndStored) {
	auto f = makePlayer(30, 300, "Newbie");
	f.commonData->setPlayerClass(model::PlayerClass::SCOUT);
	f.commonData->setGender(model::Gender::MALE);
	f.commonData->setX(10.5f);
	f.commonData->setY(20.25f);
	f.commonData->setZ(30.0f);
	f.commonData->setHeading(12);
	f.commonData->setMapId(220010000);
	bool saved = false;
	SKIP_IF_UNPORTED(saved = PlayerDAO::saveNewPlayer(*f.player, 300, "account300"));
	ASSERT_TRUE(saved);
	EXPECT_EQ(queryString("SELECT CONCAT(name, ',', account_name, ',', race, ',', player_class, ',', gender, ',', world_id, ',', x, ',', online) FROM players WHERE id = 30"),
		"Newbie,account300,ELYOS,SCOUT,MALE,220010000,10.5,0");
	EXPECT_FALSE(PlayerDAO::saveNewPlayer(*f.player, 300, "account300")) << "duplicate id: logged, false";

	f.commonData->setNote("stored note");
	f.commonData->setTitleId(400);
	// storePlayer reads the position of the spawned player (Java: PlayerService.getPlayer sets it)
	f.player->setPosition(world::WorldPosition::create(220010000, 11.0f, 21.0f, 31.0f, 13));
	SKIP_IF_UNPORTED(static_cast<void>(f.player->getWorldId()));
	PlayerDAO::storePlayer(*f.player);
	EXPECT_EQ(queryString("SELECT note FROM players WHERE id = 30"), "stored note");
	EXPECT_EQ(queryLong("SELECT title_id FROM players WHERE id = 30"), 400);
	EXPECT_EQ(queryString("SELECT CONCAT(world_id, ',', x, ',', heading) FROM players WHERE id = 30"), "220010000,11,13") << "the position, not the common data";

	PlayerDAO::onlinePlayer(*f.player, true);
	EXPECT_TRUE(PlayerDAO::isOnline(30));
	PlayerDAO::storeLastOnlineTime(30, detail::toTimestamp(1757894400000));
	PlayerDAO::storeCreationTime(30, std::nullopt);
	EXPECT_EQ(queryLong("SELECT UNIX_TIMESTAMP(last_online) FROM players WHERE id = 30"), 1757894400);
	EXPECT_FALSE(queryString("SELECT creation_date FROM players WHERE id = 30"));
}

TEST_F(PlayerDaoTest, InactiveAccountsWithLegionInfo) {
	insertPlayer(40, "Idle", 400, "ELYOS", 10);
	insertPlayer(41, "IdleLegionary", 401, "ELYOS", 10);
	insertPlayer(42, "Active", 402, "ELYOS", 10);
	insertPlayer(43, "IdleButStrong", 403, "ELYOS", 100000);
	execute("UPDATE players SET last_online = NOW() - INTERVAL 100 DAY WHERE id <> 42");
	execute("UPDATE players SET last_online = NOW() WHERE id = 42");
	execute("INSERT INTO legions (id, name) VALUES (500, 'Legion')");
	execute("INSERT INTO legion_members (legion_id, player_id, `rank`) VALUES (500, 41, 'CENTURION')");
	std::vector<PlayerDAO::PlayerAndLegionInfo> players = PlayerDAO::getPlayersOnInactiveAccounts(1000, 60);
	ASSERT_EQ(players.size(), 2u);
	std::unordered_map<int32_t, PlayerDAO::PlayerAndLegionInfo> byId;
	for (const PlayerDAO::PlayerAndLegionInfo& info : players)
		byId.emplace(info.playerId(), info);
	EXPECT_EQ(byId.at(40).legionId(), 0) << "LEFT JOIN without a legion";
	EXPECT_FALSE(byId.at(40).legionRank().has_value()) << "Java: a null rank (header request dao-2)";
	EXPECT_EQ(byId.at(41).name(), "IdleLegionary");
	EXPECT_EQ(byId.at(41).legionId(), 500);
	EXPECT_EQ(byId.at(41).legionRank(), model::team::legion::LegionRank::CENTURION);
}

TEST_F(PlayerDaoTest, AbyssRankInsertUpdateAndGp) {
	insertPlayer(50, "Fighter", 500);
	Ref<model::gameobjects::player::AbyssRank> fresh = AbyssRankDAO::loadAbyssRank(50);
	ASSERT_TRUE(fresh);
	EXPECT_EQ(fresh->getPersistentState(), Persistable::PersistentState::NEW);
	EXPECT_EQ(fresh->getRank(), utils::stats::AbyssRankEnum::GRADE9_SOLDIER);

	auto f = makePlayer(50, 500, "Fighter");
	f.player->setAbyssRank(fresh);
	EXPECT_TRUE(AbyssRankDAO::storeAbyssRank(*f.player));
	EXPECT_EQ(fresh->getPersistentState(), Persistable::PersistentState::UPDATED);
	EXPECT_EQ(queryLong("SELECT `rank` FROM abyss_rank WHERE player_id = 50"), 1) << "AbyssRankEnum.getId()";
	EXPECT_FALSE(AbyssRankDAO::storeAbyssRank(*f.player)) << "UPDATED: nothing to store";

	AbyssRankDAO::updateAbyssRank(50, utils::stats::AbyssRankEnum::STAR1_OFFICER);
	AbyssRankDAO::addGp(50, 100, true);
	AbyssRankDAO::addGp(50, -30, false);
	Ref<model::gameobjects::player::AbyssRank> loaded = AbyssRankDAO::loadAbyssRank(50);
	EXPECT_EQ(loaded->getPersistentState(), Persistable::PersistentState::UPDATED);
	EXPECT_EQ(loaded->getRank(), utils::stats::AbyssRankEnum::STAR1_OFFICER);
	EXPECT_EQ(loaded->getCurrentGP(), 70);
	EXPECT_EQ(loaded->getDailyGP(), 100);
	EXPECT_EQ(loaded->getWeeklyGP(), 100);
	AbyssRankDAO::addGp(50, -500, false);
	EXPECT_EQ(AbyssRankDAO::loadAbyssRank(50)->getCurrentGP(), 0) << "GREATEST(gp + ?, 0)";

	// updateRank through storeAbyssRank
	f.player->setAbyssRank(loaded);
	loaded->setPersistentState(Persistable::PersistentState::UPDATE_REQUIRED);
	EXPECT_TRUE(AbyssRankDAO::storeAbyssRank(*f.player));
	EXPECT_EQ(queryLong("SELECT gp FROM abyss_rank WHERE player_id = 50"), 70) << "the stored object's value";
}

TEST_F(PlayerDaoTest, RankingListUpdateUsesPlainSqlInTheBatch) {
	insertPlayer(60, "E1", 600, "ELYOS");
	insertPlayer(61, "E2", 601, "ELYOS");
	insertPlayer(62, "E3", 602, "ELYOS");
	insertPlayer(63, "A1", 603, "ASMODIANS");
	execute("UPDATE players SET last_online = NOW()");
	execute("UPDATE players SET last_online = NOW() - INTERVAL 30 DAY WHERE id = 62");
	execute("INSERT INTO abyss_rank (player_id, daily_ap, weekly_ap, ap, `rank`, daily_kill, weekly_kill, last_kill, last_ap, last_update, gp) VALUES "
			"(60, 0, 0, 1000, 10, 0, 0, 0, 0, 0, 500), (61, 0, 0, 2000, 10, 0, 0, 0, 0, 0, 900), (62, 0, 0, 3000, 10, 0, 0, 0, 0, 0, 5000), "
			"(63, 0, 0, 4000, 9, 0, 0, 0, 0, 0, 100)");
	AbyssRankDAO::updateRankingLists(7, 10, 10);
	// players offline for more than 7 days are left out, the others are numbered per race by GP
	EXPECT_EQ(queryLong("SELECT rank_pos FROM abyss_rank WHERE player_id = 61"), 1);
	EXPECT_EQ(queryLong("SELECT rank_pos FROM abyss_rank WHERE player_id = 60"), 2);
	EXPECT_EQ(queryLong("SELECT rank_pos FROM abyss_rank WHERE player_id = 62"), 0);
	EXPECT_EQ(queryLong("SELECT rank_pos FROM abyss_rank WHERE player_id = 63"), 1) << "@a = 0 before the second race";

	std::optional<std::vector<AbyssRankDAO::RankingListPlayerGp>> gpList = AbyssRankDAO::loadRankingListPlayersGp(model::Race::ELYOS);
	ASSERT_TRUE(gpList);
	ASSERT_EQ(gpList->size(), 2u);
	EXPECT_TRUE((*gpList)[0].equals(AbyssRankDAO::RankingListPlayerGp(1, 61, 900)));
	EXPECT_TRUE((*gpList)[1].equals(AbyssRankDAO::RankingListPlayerGp(2, 60, 500)));
	std::optional<std::unordered_map<int32_t, int32_t>> unranked =
		AbyssRankDAO::loadApOfPlayersNotInRankingList(model::Race::ELYOS, utils::stats::AbyssRankEnum::GRADE1_SOLDIER);
	ASSERT_TRUE(unranked);
	EXPECT_EQ(*unranked, (std::unordered_map<int32_t, int32_t>{{62, 3000}}));

	// the second update moves the positions to old_rank_pos; without an offline limit, player 62 is ranked
	AbyssRankDAO::updateRankingLists(0, 10, 10);
	EXPECT_EQ(queryLong("SELECT old_rank_pos FROM abyss_rank WHERE player_id = 61"), 1);
	EXPECT_EQ(queryLong("SELECT rank_pos FROM abyss_rank WHERE player_id = 62"), 1);

	// daily GP loss of the ranks with a configured loss (none configured here: GREATEST(gp - 0, 0))
	AbyssRankDAO::dailyUpdateGp(utils::stats::AbyssRankEnum::STAR2_OFFICER);
	EXPECT_EQ(queryLong("SELECT gp FROM abyss_rank WHERE player_id = 62"), 5000);

	// legions: ranked by contribution points among legions whose brigade general has the race
	execute("INSERT INTO legions (id, name, level, contribution_points) VALUES (700, 'Big', 5, 9000), (701, 'Small', 2, 10)");
	execute("INSERT INTO legion_members (legion_id, player_id, `rank`) VALUES (700, 60, 'BRIGADE_GENERAL'), (700, 61, 'VOLUNTEER'), (701, 63, 'BRIGADE_GENERAL')");
	AbyssRankDAO::updateRankingLists(0, 10, 10);
	std::vector<Ref<AbyssRankDAO::RankingListLegion>> legions = AbyssRankDAO::loadRankingListLegions();
	ASSERT_EQ(legions.size(), 2u);
	for (const Ref<AbyssRankDAO::RankingListLegion>& legion : legions) {
		EXPECT_EQ(legion->position(), 1) << "first of its race";
		if (legion->id() == 700) {
			EXPECT_EQ(legion->name(), "Big");
			EXPECT_EQ(legion->race(), model::Race::ELYOS);
			EXPECT_EQ(legion->memberCount(), 2);
			EXPECT_EQ(legion->contributionPoints(), 9000);
		} else {
			EXPECT_EQ(legion->race(), model::Race::ASMODIANS);
			EXPECT_EQ(legion->memberCount(), 1);
		}
	}
}

TEST_F(PlayerDaoTest, VeteranRewardsAndAdvent) {
	insertPlayer(70, "Veteran", 700);
	auto f = makePlayer(70, 700, "Veteran");
	EXPECT_EQ(VeteranRewardDAO::loadReceivedMonths(*f.player), 0);
	EXPECT_TRUE(VeteranRewardDAO::storeReceivedMonths(*f.player, 3));
	EXPECT_TRUE(VeteranRewardDAO::storeReceivedMonths(*f.player, 4));
	EXPECT_EQ(VeteranRewardDAO::loadReceivedMonths(*f.player), 4);

	using namespace std::chrono;
	const year_month_day day{year{2025}, month{12}, std::chrono::day{5}};
	EXPECT_TRUE(AdventDAO::canReceiveReward(*f.player, day)) << "no row";
	EXPECT_TRUE(AdventDAO::storeLastReceivedDay(*f.player, day));
	EXPECT_EQ(queryString("SELECT last_day_received FROM advent WHERE account_id = 700"), "2025-12-05");
	EXPECT_FALSE(AdventDAO::canReceiveReward(*f.player, day)) << "same day";
	EXPECT_TRUE(AdventDAO::canReceiveReward(*f.player, year_month_day{year{2025}, month{12}, std::chrono::day{6}}));
}

TEST_F(PlayerDaoTest, CraftAndHouseObjectCooldowns) {
	insertPlayer(80, "Cooled", 800);
	auto f = makePlayer(80, 800, "Cooled");
	const int64_t future = nowMillis() + 3600000;
	f.player->getCraftCooldowns()->put(1, future);
	f.player->getCraftCooldowns()->put(2, nowMillis() - 1000); // expired: not stored
	CraftCooldownsDAO::storeCraftCooldowns(*f.player);
	EXPECT_EQ(queryLong("SELECT COUNT(*) FROM craft_cooldowns WHERE player_id = 80"), 1);
	CraftCooldownsDAO::storeCraftCooldowns(*f.player);
	EXPECT_EQ(queryLong("SELECT COUNT(*) FROM craft_cooldowns WHERE player_id = 80"), 1) << "the old rows are deleted first";

	// house object cooldowns reference registered house objects (foreign key)
	execute("INSERT INTO player_registered_items (player_id, item_unique_id, item_id) VALUES (80, 900, 1), (80, 901, 2)");
	f.player->getHouseObjectCooldowns()->put(900, future);
	HouseObjectCooldownsDAO::storeHouseObjectCooldowns(*f.player);
	execute("INSERT INTO house_object_cooldowns (player_id, object_id, reuse_time) VALUES (80, 901, 1)");

	auto reloaded = makePlayer(80, 800, "Cooled");
	CraftCooldownsDAO::loadCraftCooldowns(*reloaded.player);
	HouseObjectCooldownsDAO::loadHouseObjectCooldowns(*reloaded.player);
	EXPECT_EQ(reloaded.player->getCraftCooldowns()->get(1), future);
	EXPECT_EQ(reloaded.player->getHouseObjectCooldowns()->get(900), future);
	EXPECT_FALSE(reloaded.player->getHouseObjectCooldowns()->get(901)) << "expired house object cooldowns are not loaded";
}

TEST_F(PlayerDaoTest, SkillAndItemCooldownsBatches) {
	insertPlayer(81, "Caster", 801);
	auto f = makePlayer(81, 801, "Caster");
	const int64_t future = nowMillis() + 3600000;
	f.player->setSkillCoolDown(5, future);
	f.player->setSkillCoolDown(6, nowMillis() + 10000); // at most 28 s left: not stored
	f.player->setSkillCoolDown(7, future + 1);
	PlayerCooldownsDAO::storePlayerCooldowns(*f.player);
	EXPECT_EQ(queryLong("SELECT COUNT(*) FROM player_cooldowns WHERE player_id = 81"), 2) << "committed batch";

	auto reloaded = makePlayer(81, 801, "Caster");
	PlayerCooldownsDAO::loadPlayerCooldowns(*reloaded.player);
	ASSERT_TRUE(reloaded.player->getSkillCoolDowns());
	EXPECT_EQ(reloaded.player->getSkillCoolDowns()->get(7), future + 1);
	EXPECT_EQ(reloaded.player->getSkillCoolDowns()->size(), 2);

	f.player->getItemCoolDowns().put(20, model::items::ItemCooldown::create(future, 300));
	f.player->getItemCoolDowns().put(21, model::items::ItemCooldown::create(nowMillis() + 20000, 5)); // at most 30 s left
	ItemCooldownsDAO::storeItemCooldowns(*f.player);
	EXPECT_EQ(queryString("SELECT CONCAT(delay_id, ',', use_delay, ',', reuse_time) FROM item_cooldowns WHERE player_id = 81"),
		"20,300," + std::to_string(future));
	EXPECT_EQ(queryLong("SELECT COUNT(*) FROM item_cooldowns"), 1);
}

TEST_F(PlayerDaoTest, PortalCooldowns) {
	insertPlayer(82, "Portal", 802);
	auto f = makePlayer(82, 802, "Portal");
	const int64_t future = nowMillis() + 3600000;
	auto cooldowns = runtime::RcHashMap<int32_t, Ref<model::gameobjects::player::PortalCooldown>>::create();
	cooldowns->put(300030000, model::gameobjects::player::PortalCooldown::create(300030000, future, 2));
	cooldowns->put(300040000, model::gameobjects::player::PortalCooldown::create(300040000, nowMillis() - 5, 1));
	f.player->getPortalCooldownList().setPortalCoolDowns(cooldowns);
	PortalCooldownsDAO::storePortalCooldowns(*f.player);
	EXPECT_EQ(queryLong("SELECT COUNT(*) FROM portal_cooldowns"), 1);

	auto reloaded = makePlayer(82, 802, "Portal");
	PortalCooldownsDAO::loadPortalCooldowns(*reloaded.player);
	auto loaded = reloaded.player->getPortalCooldownList().getPortalCoolDowns();
	ASSERT_TRUE(loaded);
	ASSERT_EQ(loaded->size(), 1);
	EXPECT_EQ(loaded->get(300030000)->getReuseTime(), future);
	EXPECT_EQ(loaded->get(300030000)->getEnterCount(), 2);
}

TEST_F(PlayerDaoTest, BindPointInsertUpdateLoad) {
	insertPlayer(83, "Bound", 803);
	auto f = makePlayer(83, 803, "Bound");
	Ref<model::gameobjects::player::BindPointPosition> bind = model::gameobjects::player::BindPointPosition::create(110010000, 1.5f, 2.5f, 3.5f, -7);
	f.player->setBindPoint(bind);
	EXPECT_EQ(bind->getPersistentState(), Persistable::PersistentState::NEW);
	EXPECT_TRUE(PlayerBindPointDAO::store(*f.player));
	EXPECT_EQ(bind->getPersistentState(), Persistable::PersistentState::UPDATED);

	Ref<model::gameobjects::player::BindPointPosition> moved = model::gameobjects::player::BindPointPosition::create(120010000, 4.0f, 5.0f, 6.0f, 90);
	moved->setPersistentState(Persistable::PersistentState::UPDATE_REQUIRED);
	f.player->setBindPoint(moved);
	EXPECT_TRUE(PlayerBindPointDAO::store(*f.player));

	auto reloaded = makePlayer(83, 803, "Bound");
	PlayerBindPointDAO::loadBindPoint(*reloaded.player);
	Ptr<model::gameobjects::player::BindPointPosition> loaded = reloaded.player->getBindPoint();
	ASSERT_TRUE(loaded);
	EXPECT_EQ(loaded->getMapId(), 120010000);
	EXPECT_EQ(loaded->getZ(), 6.0f);
	EXPECT_EQ(loaded->getHeading(), 90);
	EXPECT_EQ(loaded->getPersistentState(), Persistable::PersistentState::UPDATED);
}

TEST_F(PlayerDaoTest, EmotionsMotionsAndTitles) {
	insertPlayer(84, "Emotive", 804);
	auto f = makePlayer(84, 804, "Emotive");
	PlayerEmotionListDAO::insertEmotion(*f.player, *model::gameobjects::player::emotion::Emotion::create(10, 0));
	PlayerEmotionListDAO::insertEmotion(*f.player, *model::gameobjects::player::emotion::Emotion::create(11, 5000));
	PlayerEmotionListDAO::deleteEmotion(84, 10);
	SKIP_IF_UNPORTED(PlayerEmotionListDAO::loadEmotions(*f.player));
	ASSERT_TRUE(f.player->getEmotions());
	std::vector<Ptr<model::gameobjects::player::emotion::Emotion>> emotions = f.player->getEmotions()->getEmotions();
	ASSERT_EQ(emotions.size(), 1u);
	EXPECT_EQ(emotions[0]->getId(), 11);
	EXPECT_EQ(emotions[0]->getExpireTime(), 5000);

	EXPECT_TRUE(MotionDAO::storeMotion(84, *model::gameobjects::player::motion::Motion::create(26, 0, false)));
	EXPECT_TRUE(MotionDAO::storeMotion(84, *model::gameobjects::player::motion::Motion::create(9, 100, false)));
	EXPECT_TRUE(MotionDAO::updateMotion(84, *model::gameobjects::player::motion::Motion::create(26, 0, true)));
	EXPECT_TRUE(MotionDAO::deleteMotion(84, 9));
	SKIP_IF_UNPORTED(MotionDAO::loadMotionList(*f.player));
	ASSERT_TRUE(f.player->getMotions().getMotions());
	EXPECT_EQ(f.player->getMotions().getMotions()->size(), 1);
	EXPECT_TRUE(f.player->getMotions().getMotions()->get(26)->isActive());

	Ref<model::gameobjects::player::title::Title> title;
	SKIP_IF_UNPORTED(title = model::gameobjects::player::title::Title::create(nullptr, 7, 0));
	EXPECT_TRUE(PlayerTitleListDAO::storeTitles(*f.player, *title));
	EXPECT_FALSE(PlayerTitleListDAO::storeTitles(*f.player, *title)) << "duplicate key";
	EXPECT_EQ(queryLong("SELECT COUNT(*) FROM player_titles WHERE player_id = 84"), 1);
	EXPECT_TRUE(PlayerTitleListDAO::removeTitle(84, 7)) << "the query's trailing ';' is accepted by the server";
	EXPECT_EQ(queryLong("SELECT COUNT(*) FROM player_titles WHERE player_id = 84"), 0);
}

TEST_F(PlayerDaoTest, NpcFactions) {
	insertPlayer(85, "Faction", 805);
	// NpcFaction.isMentor reads the faction template
	PublishedHolder factionData(dataholders::DataManager::NPC_FACTIONS_DATA,
		bindXml<dataholders::NpcFactionsData>(R"(<npc_factions><npc_faction id="3" name="Radiant Ops" name_id="1129001" category="DAILY" min_level="40" race="ELYOS"/></npc_factions>)"));
	auto f = makePlayer(85, 805, "Faction");
	f.player->setNpcFactions(std::make_unique<model::gameobjects::player::npcFaction::NpcFactions>(*f.player));
	using model::gameobjects::player::npcFaction::ENpcFactionQuestState;
	Ref<model::gameobjects::player::npcFaction::NpcFaction> faction =
		model::gameobjects::player::npcFaction::NpcFaction::create(3, 1000, true, ENpcFactionQuestState::START, 4000);
	SKIP_IF_UNPORTED(f.player->getNpcFactions().addNpcFaction(*faction));
	PlayerNpcFactionsDAO::storeNpcFactions(*f.player);
	EXPECT_EQ(queryString("SELECT CONCAT(faction_id, ',', active, ',', time, ',', state, ',', quest_id) FROM player_npc_factions WHERE player_id = 85"),
		"3,1,1000,START,4000");

	auto reloaded = makePlayer(85, 805, "Faction");
	PlayerNpcFactionsDAO::loadNpcFactions(*reloaded.player);
	std::vector<Ptr<model::gameobjects::player::npcFaction::NpcFaction>> factions = reloaded.player->getNpcFactions().getNpcFactions();
	ASSERT_EQ(factions.size(), 1u);
	EXPECT_EQ(factions[0]->getState(), ENpcFactionQuestState::START);
	EXPECT_EQ(factions[0]->getPersistentState(), Persistable::PersistentState::UPDATED);
}

TEST_F(PlayerDaoTest, AppearanceRoundTrip) {
	insertPlayer(86, "Pretty", 806);
	Ref<model::gameobjects::player::PlayerAppearance> pa = model::gameobjects::player::PlayerAppearance::create();
	pa->setFace(1);
	pa->setHair(2);
	pa->setSkinRGB(0x00ffeedd);
	pa->setLipRGB(12);
	pa->setEyeRGB(13);
	pa->setJawHeigh(34);
	pa->setVoice(53);
	pa->setHeight(1.25f);
	EXPECT_TRUE(PlayerAppearanceDAO::store(86, *pa));
	EXPECT_EQ(queryLong("SELECT lip_rgb FROM player_appearance WHERE player_id = 86"), 12) << "the Java parameter order (lip before eye)";
	Ref<model::gameobjects::player::PlayerAppearance> loaded = PlayerAppearanceDAO::load(86);
	ASSERT_TRUE(loaded);
	EXPECT_EQ(loaded->getFace(), 1);
	EXPECT_EQ(loaded->getSkinRGB(), 0x00ffeedd);
	EXPECT_EQ(loaded->getLipRGB(), 12);
	EXPECT_EQ(loaded->getEyeRGB(), 13);
	EXPECT_EQ(loaded->getJawHeigh(), 34);
	EXPECT_EQ(loaded->getVoice(), 53);
	EXPECT_EQ(loaded->getHeight(), 1.25f);
	EXPECT_EQ(PlayerAppearanceDAO::load(87)->getFace(), 0) << "no row: a default appearance";
}

TEST_F(PlayerDaoTest, Punishments) {
	using services::PunishmentService_PunishmentType;
	insertPlayer(88, "Prisoner", 808);
	PlayerPunishmentsDAO::punishPlayer(88, PunishmentService_PunishmentType::CHARBAN, 3600, "botting");
	PlayerPunishmentsDAO::punishPlayer(88, PunishmentService_PunishmentType::PRISON, 600, "spam");
	Ref<model::account::CharacterBanInfo> ban = PlayerPunishmentsDAO::getCharBanInfo(88);
	ASSERT_TRUE(ban);
	EXPECT_EQ(queryLong("SELECT duration FROM player_punishments WHERE punishment_type = 'CHARBAN'"), 3600);
	EXPECT_FALSE(PlayerPunishmentsDAO::getCharBanInfo(89));

	auto f = makePlayer(88, 808, "Prisoner");
	PlayerPunishmentsDAO::loadPlayerPunishments(*f.player);
	const int64_t expectedEnd = nowMillis() + 600000;
	PlayerPunishmentsDAO::unpunishPlayer(88, PunishmentService_PunishmentType::CHARBAN);
	EXPECT_FALSE(PlayerPunishmentsDAO::getCharBanInfo(88));
	int32_t prisonSeconds = 0;
	SKIP_IF_UNPORTED(prisonSeconds = f.player->getPrisonDurationSeconds());
	EXPECT_NEAR(prisonSeconds, 600, 5) << "end time " << expectedEnd;
	PlayerPunishmentsDAO::storePlayerPunishment(*f.player, PunishmentService_PunishmentType::PRISON);
	EXPECT_NEAR(static_cast<double>(*queryLong("SELECT duration FROM player_punishments WHERE punishment_type = 'PRISON'")), 600.0, 5.0);
}

TEST_F(PlayerDaoTest, SettingsBlobs) {
	insertPlayer(90, "Settings", 810);
	execute("INSERT INTO player_settings VALUES (90, 0, x'0102ff'), (90, 1, x''), (90, -1, 7), (90, -2, 3)");
	Ref<model::gameobjects::player::PlayerSettings> settings;
	SKIP_IF_UNPORTED(settings = PlayerSettingsDAO::loadSettings(90));
	EXPECT_EQ(settings->getPersistentState(), Persistable::PersistentState::UPDATED);
	ASSERT_TRUE(settings->getUiSettings());
	EXPECT_EQ(settings->getUiSettings()->length(), 3);
	EXPECT_EQ(settings->getUiSettings()->get(2), -1);
	ASSERT_TRUE(settings->getShortcuts());
	EXPECT_EQ(settings->getShortcuts()->length(), 0);
	EXPECT_FALSE(settings->getHouseBuddies());
	EXPECT_EQ(settings->getDisplay(), 7);
	EXPECT_EQ(settings->getDeny(), 3);

	auto f = makePlayer(90, 810, "Settings");
	settings->setHouseBuddies(runtime::Array<int8_t>::of({9, 8}));
	settings->setDeny(5);
	settings->setPersistentState(Persistable::PersistentState::UPDATE_REQUIRED);
	f.player->setPlayerSettings(settings);
	PlayerSettingsDAO::saveSettings(*f.player);
	EXPECT_EQ(queryString("SELECT HEX(settings) FROM player_settings WHERE player_id = 90 AND settings_type = 2"), "0908");
	EXPECT_EQ(queryString("SELECT HEX(settings) FROM player_settings WHERE player_id = 90 AND settings_type = 0"), "0102FF");
	EXPECT_EQ(queryString("SELECT settings FROM player_settings WHERE player_id = 90 AND settings_type = -2"), "5");
}

TEST_F(PlayerDaoTest, SkillListStore) {
	insertPlayer(91, "Skilled", 811);
	execute("INSERT INTO player_skills (player_id, skill_id, skill_level) VALUES (91, 100, 1), (91, 101, 2)");
	Ref<model::skill::PlayerSkillList> skills = PlayerSkillListDAO::loadSkillList(91);
	std::vector<Ptr<model::skill::PlayerSkillEntry>> all;
	SKIP_IF_UNPORTED(all = skills->getAllSkills());
	ASSERT_EQ(all.size(), 2u);
	EXPECT_EQ(skills->getSkillEntry(101)->getSkillLevel(), 2);
	EXPECT_EQ(skills->getSkillEntry(101)->getPersistentState(), Persistable::PersistentState::UPDATED);

	// Java PlayerSkillListDAO.store: deleted, new and changed entries in one connection (three committed batches)
	auto f = makePlayer(91, 811, "Skilled");
	Ref<model::skill::PlayerSkillEntry> added = model::skill::PlayerSkillEntry::create(102, 3, 0, Persistable::PersistentState::NEW);
	Ref<model::skill::PlayerSkillEntry> changed = model::skill::PlayerSkillEntry::create(100, 5, 0, Persistable::PersistentState::UPDATE_REQUIRED);
	Ref<model::skill::PlayerSkillEntry> removed = model::skill::PlayerSkillEntry::create(101, 2, 0, Persistable::PersistentState::DELETED);
	f.player->setSkillList(model::skill::PlayerSkillList::create({Ptr<model::skill::PlayerSkillEntry>(added), Ptr<model::skill::PlayerSkillEntry>(changed),
		Ptr<model::skill::PlayerSkillEntry>(removed)}));
	EXPECT_TRUE(PlayerSkillListDAO::storeSkills(*f.player));
	EXPECT_EQ(queryString("SELECT GROUP_CONCAT(CONCAT(skill_id, ':', skill_level) ORDER BY skill_id) FROM player_skills WHERE player_id = 91"),
		"100:5,102:3");
	EXPECT_EQ(added->getPersistentState(), Persistable::PersistentState::UPDATED);
}

TEST_F(PlayerDaoTest, RankingListPlayersAndRaceCountUseTheExperienceTable) {
	std::string table = "<player_experience_table>";
	for (int32_t level = 0; level <= 80; level++)
		table += "<exp>" + std::to_string(int64_t{level} * 1000) + "</exp>";
	PublishedHolder experience(dataholders::DataManager::PLAYER_EXPERIENCE_TABLE, bindXml<dataholders::PlayerExperienceTable>(table + "</player_experience_table>"));

	insertPlayer(100, "Ranked", 1000, "ELYOS", 45500, "CHANTER");
	insertPlayer(101, "Second", 1000, "ELYOS", 10000);
	insertPlayer(102, "Asmo", 1001, "ASMODIANS", 60000);
	execute("UPDATE players SET gender = 'FEMALE', title_id = 12 WHERE id = 100");
	execute("INSERT INTO abyss_rank (player_id, daily_ap, weekly_ap, ap, `rank`, daily_kill, weekly_kill, last_kill, last_ap, last_update, gp, rank_pos, old_rank_pos) "
			"VALUES (100, 0, 0, 777, 12, 0, 0, 0, 0, 0, 55, 3, 4)");
	execute("INSERT INTO legions (id, name) VALUES (2000, 'Guild')");
	execute("INSERT INTO legion_members (legion_id, player_id) VALUES (2000, 100)");
	std::vector<Ref<AbyssRankDAO::RankingListPlayer>> players = AbyssRankDAO::loadRankingListPlayers();
	ASSERT_EQ(players.size(), 1u) << "rank_pos > 0";
	// PlayerExperienceTable.getLevelForExp(45500): the highest i with exp >= table[i - 1] = (i - 1) * 1000, i = 46
	EXPECT_TRUE(players[0]->equals(*AbyssRankDAO::RankingListPlayer::create(3, 4, 100, "Ranked", model::Race::ELYOS, 46, 12, 777, 55, 12,
		model::PlayerClass::CHANTER, model::Gender::FEMALE, "Guild")))
		<< "level " << players[0]->level() << ", legion " << players[0]->legionName();

	// accounts with a character of the race at or above the level of GSConfig.RATIO_MIN_REQUIRED_LEVEL
	const int32_t previous = configs::main::GSConfig::RATIO_MIN_REQUIRED_LEVEL.load();
	configs::main::GSConfig::RATIO_MIN_REQUIRED_LEVEL.store(20);
	EXPECT_EQ(PlayerDAO::getCharacterCountForRace(model::Race::ELYOS), 1) << "account 1000 once (DISTINCT), player 101 is below level 20";
	EXPECT_EQ(PlayerDAO::getCharacterCountForRace(model::Race::ASMODIANS), 1);
	configs::main::GSConfig::RATIO_MIN_REQUIRED_LEVEL.store(previous);
}

TEST_F(PlayerDaoTest, QuestStateListLoadAndStore) {
	using questEngine::model::QuestState;
	using questEngine::model::QuestStatus;
	insertPlayer(110, "Quester", 1100);
	// the model bodies the DAO calls inside its catch-all blocks
	SKIP_IF_UNPORTED(QuestState::create(1, QuestStatus::START, 0, 0, 0, std::nullopt, std::nullopt, std::nullopt)->setPersistentState(Persistable::PersistentState::UPDATED));
	SKIP_IF_UNPORTED(static_cast<void>(QuestState::create(1, QuestStatus::START, 5, 0, 0, std::nullopt, std::nullopt, std::nullopt)->getQuestVars()->getQuestVars()));

	execute("INSERT INTO player_quests (player_id, quest_id, status, quest_vars, flags, complete_count, next_repeat_time, reward, complete_time) VALUES "
			"(110, 1000, 'START', 3, 1, 0, NULL, NULL, NULL), (110, 1001, 'COMPLETE', 0, 0, 2, '2025-09-15 10:00:00', 1, '2025-09-14 10:00:00')");
	Ref<model::gameobjects::player::QuestStateList> list = PlayerQuestListDAO::load(110);
	Ptr<QuestState> started = list->getQuestState(1000);
	Ptr<QuestState> completed = list->getQuestState(1001);
	ASSERT_TRUE(started && completed);
	EXPECT_EQ(started->getStatus(), QuestStatus::START);
	EXPECT_FALSE(started->getRewardGroup()) << "wasNull";
	EXPECT_EQ(completed->getRewardGroup(), 1);
	EXPECT_EQ(completed->getCompleteCount(), 2);
	ASSERT_TRUE(completed->getNextRepeatTime());

	auto f = makePlayer(110, 1100, "Quester");
	f.player->setQuestStateList(list);
	Ref<QuestState> added = QuestState::create(1002, QuestStatus::REWARD, 7, 0, 1, std::nullopt, 2, std::nullopt);
	added->setPersistentState(Persistable::PersistentState::NEW);
	list->addQuest(1002, *added);
	completed->setPersistentState(Persistable::PersistentState::DELETED);
	started->setPersistentState(Persistable::PersistentState::UPDATE_REQUIRED);
	PlayerQuestListDAO::store(*f.player);
	EXPECT_EQ(queryString("SELECT GROUP_CONCAT(CONCAT(quest_id, ':', status, ':', IFNULL(reward, '-')) ORDER BY quest_id) FROM player_quests"),
		"1000:START:-,1002:REWARD:2");
}

TEST_F(PlayerDaoTest, QuestIdDeletedDuringTheStoreStaysForTheNextStore) {
	// D6 fix of the Java race in PlayerQuestListDAO.store/deleteQuest (docs/deviations/P4-14.md): the ids of the DELETE batch are removed from the
	// player's deleted-quest set afterwards, not the whole set, so an id added while the batch runs is deleted by the next store. The batch is
	// held at its first DELETE by a row lock of a second connection; the test adds the new id once the server reports the DELETE waiting.
	using commons::database::Connection;
	using commons::database::ConnectionProperties;
	insertPlayer(130, "Abandoner", 1300);
	auto f = makePlayer(130, 1300, "Abandoner");
	Ref<model::gameobjects::player::QuestStateList> list = PlayerQuestListDAO::load(130); // no rows yet: no QuestState bodies of other chunks
	ASSERT_TRUE(list);
	execute("INSERT INTO player_quests (player_id, quest_id, status) VALUES (130, 1001, 'START'), (130, 1005, 'START')");
	f.player->setQuestStateList(list);
	list->getDeletedQuestIds().add(1001);

	std::unique_ptr<Connection> locker = Connection::open(ConnectionProperties::parse(urlWithDatabase(TEST_DATABASE), user(), password()));
	locker->setAutoCommit(false);
	static_cast<void>(locker->prepareStatement("SELECT quest_id FROM player_quests WHERE player_id = 130 AND quest_id = 1001 FOR UPDATE")->executeQuery());

	std::thread store([&] {
		runtime::TaskScope storeScope(AION_TASK_INFO(runtime::TaskKind::TEST));
		PlayerQuestListDAO::store(*f.player);
	});
	bool waiting = false;
	for (int attempt = 0; attempt < 400 && !waiting; ++attempt) {
		// the DELETE statement of the batch is executing (it waits for the row lock): the store has taken its snapshot of the deleted ids
		waiting = queryLong("SELECT COUNT(*) FROM information_schema.PROCESSLIST WHERE INFO LIKE 'DELETE FROM `player_quests`%'").value_or(0) > 0;
		if (!waiting)
			std::this_thread::sleep_for(std::chrono::milliseconds(25));
	}
	list->getDeletedQuestIds().add(1005); // after the snapshot of the running store: the player abandons another quest
	locker->rollback();
	store.join();
	ASSERT_TRUE(waiting) << "the DELETE of the store never waited for the row lock";

	EXPECT_EQ(queryString("SELECT GROUP_CONCAT(quest_id ORDER BY quest_id) FROM player_quests"), "1005") << "1001 deleted by the first store";
	EXPECT_EQ(list->getDeletedQuestIds().snapshot(), (std::vector<int32_t>{1005})) << "Java's questIds.clear() would drop 1005 here";
	PlayerQuestListDAO::store(*f.player);
	EXPECT_EQ(queryLong("SELECT COUNT(*) FROM player_quests"), 0) << "the next store deletes 1005";
	EXPECT_TRUE(list->getDeletedQuestIds().snapshot().empty());
}

TEST_F(PlayerDaoTest, EffectsOfAPlayerWithoutEffects) {
	insertPlayer(120, "Buffed", 1200);
	execute("INSERT INTO player_effects (player_id, skill_id, skill_lvl, remaining_time, end_time) VALUES (120, 1, 1, 60000, 0)");
	auto f = makePlayer(120, 1200, "Buffed");
	if (!f.player->getEffectController())
		GTEST_SKIP() << "the test player has no effect controller (Player::postConstruct stops at the unported PlayerGameStats constructor, P5-01)";
	SKIP_IF_UNPORTED(static_cast<void>(f.player->getEffectController()->getAbnormalEffects()));
	PlayerEffectsDAO::storePlayerEffects(*f.player);
	EXPECT_EQ(queryLong("SELECT COUNT(*) FROM player_effects"), 0) << "the old effects are deleted, a player without effects stores none";
}

TEST_F(PlayerDaoTest, ChallengeTasksWithoutRows) {
	EXPECT_TRUE(ChallengeTasksDAO::load(3000, model::templates::challenge::ChallengeType::LEGION).empty());
}

TEST_F(PlayerDaoTest, FriendsAndBlocks) {
	insertPlayer(92, "Friendly", 812);
	insertPlayer(93, "Buddy", 813);
	auto a = makePlayer(92, 812, "Friendly");
	auto b = makePlayer(93, 813, "Buddy");
	EXPECT_TRUE(FriendListDAO::addFriends(*a.player, *b.player));
	EXPECT_EQ(queryLong("SELECT COUNT(*) FROM friends"), 2) << "both directions in one batch";
	EXPECT_TRUE(FriendListDAO::setFriendMemo(92, 93, "old pal"));
	EXPECT_EQ(queryString("SELECT memo FROM friends WHERE player = 92"), "old pal");
	EXPECT_EQ(queryString("SELECT memo FROM friends WHERE player = 93"), "");
	EXPECT_TRUE(FriendListDAO::delFriends(93, 92));
	EXPECT_EQ(queryLong("SELECT COUNT(*) FROM friends"), 0);

	EXPECT_TRUE(BlockListDAO::addBlockedUser(92, 93, "rude"));
	EXPECT_FALSE(BlockListDAO::addBlockedUser(92, 93, "rude")) << "duplicate key";
	EXPECT_TRUE(BlockListDAO::setReason(92, 93, "very rude"));
	EXPECT_EQ(queryString("SELECT reason FROM blocks WHERE player = 92"), "very rude");
	EXPECT_TRUE(BlockListDAO::delBlockedUser(92, 93));
	EXPECT_EQ(queryLong("SELECT COUNT(*) FROM blocks"), 0);
}

} // namespace
} // namespace aion::gameserver::dao::test
