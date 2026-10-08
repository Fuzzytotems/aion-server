// P5-08 PunishmentService, PvpService's headhunting registry, AbyssService.announceAbyssSkillUsage and AbyssRankUpdateService (m5j-plan.md
// §18.1 stage 1 CP2, items S-03 to S-05): a character ban's row and the kick of an online character, the unban, the duration rule, the gather
// captcha's three arms, the abyss skill announcement's recipients, the headhunter getters and the season's end, and the GP rank update against
// the DAO test database. Real Players of the party fixture (tests/team/P5-10b, by relative path, as SocialDuelServiceTest).
//
// Expectations are derived by hand from PunishmentService.java:30-150, PvpService.java:86-97, :276-278, AbyssService.java:33-36 and
// AbyssRankUpdateService.java:39-140 (the GP ranks from AbyssRankEnum.java's table and RankingConfig's quotas the cases set).
//
// Not driven here: setIsInPrison (the prison teleport and the bind point move need the prison and bind data; the stage-1 gate's //sprison case
// drives them, m5j-plan.md §18.1 CP5) and AbyssRankUpdateService.updateDailyGpLoss (private, reached only from its cron schedule).

#include "../team/P5-10b/TeamTestSupport.h"

#include "../dao/DaoTestDatabase.h"

#include <chrono>
#include <cstdint>
#include <limits>
#include <map>
#include <string>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/main/RankingConfig.h"
#include "aion/gameserver/dataholders/KillBountyData.bind.h"
#include "aion/gameserver/dataholders/KillBountyData.h"
#include "aion/gameserver/model/event/Headhunter.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CAPTCHA.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUIT_RESPONSE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/fields/Array.h"
#include "aion/gameserver/services/PunishmentService.h"
#include "aion/gameserver/services/PvpService.h"
#include "aion/gameserver/services/abyss/AbyssRankUpdateService.h"
#include "aion/gameserver/services/abyss/AbyssService.h"
#include "aion/gameserver/world/World.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing::team {
namespace {

using serverpackets::SM_CAPTCHA;
using serverpackets::SM_QUIT_RESPONSE;
using services::PunishmentService;
using PunishmentType = services::PunishmentService_PunishmentType;
using RankConfig = configs::detail::AbyssRankEnum;

class PunishmentAbyssServiceTest : public TeamTest {
protected:
	void TearDown() override {
		for (Member& m : members)
			world::World::getInstance().removeObject(*m.f.player);
		configs::main::RankingConfig::TOP_RANKING_QUOTA.set({});
		TeamTest::TearDown();
		if (bountiesPublished)
			dataholders::DataManager::KILL_BOUNTY_DATA.resetForTests();
	}

	Member& addStoredMember(std::string_view name, float x = 100.0f) {
		Member& m = addMember(name, x);
		world::World::getInstance().storeObject(*m.f.player);
		return m;
	}

	bool requireDatabase() {
		if (!dao::test::isEnabled())
			return false;
		dao::test::setUpDatabaseOnce();
		dao::test::clearTables();
		return true;
	}

	/** PvpService's constructor reads the kill bounties (and the headhunters of the database) */
	void publishBounties() {
		xml::LoadContext context;
		dataholders::DataManager::KILL_BOUNTY_DATA.publish(xml::bindString<dataholders::KillBountyData>(context, "<kill_bounties/>"));
		bountiesPublished = true;
	}

	static std::string punishmentRow(int32_t playerId, std::string_view type, std::string_view column) {
		return dao::test::queryString("SELECT " + std::string(column) + " FROM player_punishments WHERE player_id = " + std::to_string(playerId) +
									  " AND punishment_type = '" + std::string(type) + "'")
			.value_or("<none>");
	}

	static int64_t rankOf(int32_t playerId) {
		return dao::test::queryLong("SELECT `rank` FROM abyss_rank WHERE player_id = " + std::to_string(playerId)).value_or(-1);
	}

	static int64_t rankPosOf(int32_t playerId) {
		return dao::test::queryLong("SELECT rank_pos FROM abyss_rank WHERE player_id = " + std::to_string(playerId)).value_or(-1);
	}

	static void insertRank(int32_t playerId, int32_t rank, int32_t ap, int32_t gp) {
		dao::test::execute("INSERT INTO abyss_rank (player_id, daily_ap, weekly_ap, ap, `rank`, daily_kill, weekly_kill, last_kill, last_ap, "
						   "last_update, gp) VALUES (" + std::to_string(playerId) + ", 0, 0, " + std::to_string(ap) + ", " + std::to_string(rank) +
						   ", 0, 0, 0, 0, 0, " + std::to_string(gp) + ")");
	}

	bool bountiesPublished = false;
};

// ---- PunishmentService ----------------------------------------------------------------------------------------------------------------------

/** PunishmentService.java:56-60: 0 days is Integer.MAX_VALUE seconds, otherwise the days in seconds */
TEST_F(PunishmentAbyssServiceTest, ADurationOfNoDaysIsTheLargestInt) {
	EXPECT_EQ(PunishmentService::calculateDuration(0), std::numeric_limits<int32_t>::max());
	EXPECT_EQ(PunishmentService::calculateDuration(1), 86'400);
	EXPECT_EQ(PunishmentService::calculateDuration(3), 259'200);
}

/** PunishmentService.java:41-48, :30-32: the CHARBAN row, the online character kicked with SM_QUIT_RESPONSE; the unban deletes the row */
TEST_F(PunishmentAbyssServiceTest, ACharacterBanStoresTheRowAndKicksTheOnlineCharacter) {
	if (!requireDatabase())
		GTEST_SKIP() << "the DAO test database is not configured (AION_TEST_GS_DATABASE_URL)";
	Member& a = addStoredMember("Alpha");
	const int32_t id = a.player().getObjectId();
	dao::test::insertPlayer(id, "Alpha", 9901);
	dao::test::insertPlayer(4242, "Offline", 9902);

	PunishmentService::banChar(id, 2, "botting");
	EXPECT_EQ(punishmentRow(id, "CHARBAN", "duration"), "172800");
	EXPECT_EQ(punishmentRow(id, "CHARBAN", "reason"), "botting");
	EXPECT_EQ(a.count(SM_QUIT_RESPONSE()), 1) << "AionConnection.close(SM_QUIT_RESPONSE)";
	EXPECT_TRUE((*a.client)->isPendingClose());

	PunishmentService::banChar(4242, 0, "forever");
	EXPECT_EQ(punishmentRow(4242, "CHARBAN", "duration"), std::to_string(std::numeric_limits<int32_t>::max())) << "an offline character";

	PunishmentService::unbanChar(id);
	EXPECT_EQ(punishmentRow(id, "CHARBAN", "duration"), "<none>");
	EXPECT_EQ(punishmentRow(4242, "CHARBAN", "duration"), std::to_string(std::numeric_limits<int32_t>::max())) << "only his row";
}

/** PunishmentService.java:41-48: an online character without a connection is Java's NPE on getClientConnection().close */
TEST_F(PunishmentAbyssServiceTest, ABanOfAnOnlineCharacterWithoutAConnectionThrows) {
	Member& a = addStoredMember("Alpha");
	a.player().setClientConnection(nullptr);
	EXPECT_THROW(PunishmentService::banChar(a.player().getObjectId(), 1, "x"), runtime::NullPointerException);
}

/** PunishmentService.java:134-142: below three captchas the next one is sent (count + 1), the gather restriction set and the GATHER row stored */
TEST_F(PunishmentAbyssServiceTest, AGatherBotSuspectGetsTheNextCaptcha) {
	const bool db = requireDatabase();
	Member& a = addStoredMember("Alpha");
	const int32_t id = a.player().getObjectId();
	if (db)
		dao::test::insertPlayer(id, "Alpha", 9901);
	a.player().setCaptchaWord("abc");
	a.player().setCaptchaImage(runtime::Array<int8_t>::of({1, 2, -3}));

	PunishmentService::setIsNotGatherable(a.player(), 1, true, 600'000);
	const uint8_t image[] = {1, 2, 0xFD};
	EXPECT_EQ(a.count(SM_CAPTCHA(2, image)), 1) << "SM_CAPTCHA(captchaCount + 1, image)";
	EXPECT_TRUE(a.player().isGatherRestricted());
	EXPECT_GE(a.player().getGatherRestrictionDurationSeconds(), 595);
	EXPECT_LE(a.player().getGatherRestrictionDurationSeconds(), 600);
	EXPECT_EQ(a.player().getCaptchaWord(), std::optional<std::string>("abc")) << "the word stays for the answer";
	if (db) {
		EXPECT_EQ(punishmentRow(id, "GATHER", "reason"), "Possible gatherbot");
		EXPECT_NE(punishmentRow(id, "GATHER", "duration"), "<none>");
	}
}

/** PunishmentService.java:137-140: the third captcha clears the word and the image and sends no packet */
TEST_F(PunishmentAbyssServiceTest, TheThirdCaptchaClearsTheWordAndTheImage) {
	Member& a = addStoredMember("Alpha");
	a.player().setCaptchaWord("abc");
	a.player().setCaptchaImage(runtime::Array<int8_t>::of({1}));
	PunishmentService::setIsNotGatherable(a.player(), 3, true, 60'000);
	EXPECT_EQ(a.count(opcodeOf<SM_CAPTCHA>), 0);
	EXPECT_EQ(a.player().getCaptchaWord(), std::nullopt);
	EXPECT_EQ(a.player().getCaptchaImage(), nullptr);
	EXPECT_TRUE(a.player().isGatherRestricted());
}

/**
 * PunishmentService.java:136: SM_CAPTCHA.writeImpl reads the image's length on the dispatcher thread, whose NPE Dispatcher.run logs; the packet
 * is lost and the restriction still set
 */
TEST_F(PunishmentAbyssServiceTest, ACaptchaWithoutAnImageIsLostButTheRestrictionStands) {
	Member& a = addStoredMember("Alpha");
	network::test::LogCapture capture({"com.aionemu.commons.network.Dispatcher"});
	PunishmentService::setIsNotGatherable(a.player(), 0, true, 60'000);
	EXPECT_EQ(a.count(opcodeOf<SM_CAPTCHA>), 0);
	EXPECT_EQ(capture.count("SM_CAPTCHA.writeImpl"), 1) << capture.dump();
	EXPECT_TRUE(a.player().isGatherRestricted());
}

/** PunishmentService.java:143-149: the recovery message, the captcha cleared, the restriction lifted and the GATHER row deleted */
TEST_F(PunishmentAbyssServiceTest, TheRecoveryLiftsTheRestriction) {
	const bool db = requireDatabase();
	Member& a = addStoredMember("Alpha");
	const int32_t id = a.player().getObjectId();
	if (db) {
		dao::test::insertPlayer(id, "Alpha", 9901);
		dao::test::execute("INSERT INTO player_punishments VALUES (" + std::to_string(id) + ", 'GATHER', 1, 600, 'Possible gatherbot')");
	}
	a.player().setCaptchaWord("abc");
	a.player().setCaptchaImage(runtime::Array<int8_t>::of({1}));
	a.player().setGatherRestrictionExpirationTime(commons::utils::currentTimeMillis() + 600'000);

	PunishmentService::setIsNotGatherable(a.player(), 0, false, 0);
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_MSG_CAPTCHA_RECOVERED()), 1);
	EXPECT_EQ(a.player().getCaptchaWord(), std::nullopt);
	EXPECT_EQ(a.player().getCaptchaImage(), nullptr);
	EXPECT_FALSE(a.player().isGatherRestricted());
	if (db)
		EXPECT_EQ(punishmentRow(id, "GATHER", "duration"), "<none>");
}

// ---- AbyssService ---------------------------------------------------------------------------------------------------------------------------

/** AbyssService.java:33-36: every player of the world type but the user, outside instances */
TEST_F(PunishmentAbyssServiceTest, AnAbyssSkillIsAnnouncedToTheOthersOfTheWorldType) {
	Member& a = addStoredMember("Alpha");
	Member& b = addStoredMember("Bravo", 110.0f);
	Member& c = addStoredMember("Charlie", 600.0f);
	services::abyss::AbyssService::announceAbyssSkillUsage(a.player(), "Abyssal Fury");
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_SKILL_ABYSS_SKILL_IS_FIRED(a.player(), "Abyssal Fury")), 0) << "not the user";
	EXPECT_EQ(b.count(SM_SYSTEM_MESSAGE::STR_SKILL_ABYSS_SKILL_IS_FIRED(a.player(), "Abyssal Fury")), 1);
	EXPECT_EQ(c.count(SM_SYSTEM_MESSAGE::STR_SKILL_ABYSS_SKILL_IS_FIRED(a.player(), "Abyssal Fury")), 1) << "not only who sees him";
}

// ---- PvpService headhunting -----------------------------------------------------------------------------------------------------------------

/** PvpService.java:94-97, :276-278, :86-88: a new headhunter is registered once, a loaded one returned; the season's end clears them */
TEST_F(PunishmentAbyssServiceTest, HeadhuntersAreRegisteredOnceAndClearedAtTheSeasonsEnd) {
	if (!requireDatabase())
		GTEST_SKIP() << "the DAO test database is not configured (AION_TEST_GS_DATABASE_URL)";
	dao::test::execute("INSERT INTO headhunting (hunter_id, accumulated_kills, last_update) VALUES (30, 5, '2025-09-01 10:00:00')");
	publishBounties();
	services::PvpService& pvp = services::PvpService::getInstance();

	EXPECT_EQ(pvp.getHeadhunter(77), nullptr);
	runtime::Ptr<model::event::Headhunter> created = pvp.getHeadhunterById(77);
	ASSERT_NE(created, nullptr);
	EXPECT_EQ(created->getHunterId(), 77);
	EXPECT_EQ(created->getKills(), 0);
	EXPECT_EQ(created->getPersistentState(), model::gameobjects::Persistable::PersistentState::UPDATE_REQUIRED);
	EXPECT_EQ(pvp.getHeadhunterById(77), created) << "putIfAbsent keeps the first";
	EXPECT_EQ(pvp.getHeadhunter(77), created);

	runtime::Ptr<model::event::Headhunter> loaded = pvp.getHeadhunterById(30);
	ASSERT_NE(loaded, nullptr);
	EXPECT_EQ(loaded->getKills(), 5) << "the loaded headhunter, not a new one";

	pvp.finalizeHeadhuntingSeason();
	EXPECT_EQ(pvp.getHeadhunter(77), nullptr);
	EXPECT_EQ(pvp.getHeadhunter(30), nullptr);
}

// ---- AbyssRankUpdateService -----------------------------------------------------------------------------------------------------------------

/**
 * AbyssRankUpdateService.java:39-128: a second after performUpdate the ranking positions are renumbered (the largest GP quota is the list's
 * length), each race's ranked players take the GP ranks from SUPREME_COMMANDER down while quota and required GP allow, and the unranked holders
 * of a GP rank fall back to the AP rank of their points (AbyssRankEnum.java: ids and required AP/GP).
 *
 * Quotas: GENERAL 1, STAR1_OFFICER 4 (every other rank 0). Elyos 101 (9000 GP) is GENERAL; 106 (8500, enough for GENERAL, whose quota is
 * used) and 102 (2000) STAR1_OFFICER; 103 (1100, below STAR1's 1244) is ranked but keeps his rank; 104 (1000 GP, 5th) holds no position and falls from STAR2_OFFICER to GRADE4_SOLDIER by his 50000 AP; 105
 * (no GP) falls from STAR3_OFFICER to GRADE9_SOLDIER. Asmodian 201 (9500) is GENERAL of his race.
 */
TEST_F(PunishmentAbyssServiceTest, TheRankUpdateHandsOutTheGpRanksByQuota) {
	if (!requireDatabase())
		GTEST_SKIP() << "the DAO test database is not configured (AION_TEST_GS_DATABASE_URL)";
	configs::main::RankingConfig::TOP_RANKING_QUOTA.set(std::map<RankConfig, int32_t>{{RankConfig::GENERAL, 1}, {RankConfig::STAR1_OFFICER, 4}});
	for (int32_t id : {101, 102, 103, 104, 105, 106})
		dao::test::insertPlayer(id, "Elyos" + std::to_string(id), 9000 + id);
	dao::test::insertPlayer(201, "Asmo201", 9201, "ASMODIANS");
	insertRank(101, 1, 0, 9000);
	insertRank(102, 1, 0, 2000);
	insertRank(106, 1, 0, 8500);
	insertRank(103, 1, 0, 1100);
	insertRank(104, 11, 50'000, 1000);
	insertRank(105, 12, 0, 0);
	insertRank(201, 1, 0, 9500);

	services::abyss::AbyssRankUpdateService::performUpdate();
	executor->advance(std::chrono::milliseconds(999));
	EXPECT_EQ(rankOf(101), 1) << "not before the second";
	executor->advance(std::chrono::milliseconds(1));

	EXPECT_EQ(rankPosOf(101), 1);
	EXPECT_EQ(rankPosOf(106), 2);
	EXPECT_EQ(rankPosOf(102), 3);
	EXPECT_EQ(rankPosOf(103), 4);
	EXPECT_EQ(rankPosOf(104), 0) << "the list holds the largest quota";
	EXPECT_EQ(rankPosOf(201), 1);

	EXPECT_EQ(rankOf(101), 15) << "GENERAL";
	EXPECT_EQ(rankOf(106), 10) << "GENERAL's quota is used: STAR1_OFFICER";
	EXPECT_EQ(rankOf(102), 10) << "STAR1_OFFICER";
	EXPECT_EQ(rankOf(103), 1) << "below the required GP: the loop stops, the rank stays";
	EXPECT_EQ(rankOf(104), 6) << "GRADE4_SOLDIER by 50000 AP";
	EXPECT_EQ(rankOf(105), 1) << "GRADE9_SOLDIER";
	EXPECT_EQ(rankOf(201), 15) << "the Asmodians' GENERAL";
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing::team
