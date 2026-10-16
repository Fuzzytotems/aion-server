// P5-08 SocialService and DuelService, and PlayerReviveService's duelRevive (m5j-plan.md §18.1 stage 1 CP1, items S-01 and S-02): the duel request,
// its two questions and their answers, the duel's start, its five-minute draw on the deterministic clock and a lost duel; the friend and block
// lists against the DAO test database. Real Players of the party fixture (tests/team/P5-10b, by relative path, as RecallServiceTest).
//
// Expectations are derived by hand from DuelService.java:50-241, SocialService.java:20-125 and PlayerReviveService.java:36-40.

#include "../team/P5-10b/TeamTestSupport.h"

#include "../dao/DaoTestDatabase.h"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

#include "aion/gameserver/dataholders/HouseData.bind.h"
#include "aion/gameserver/dataholders/HouseData.h"
#include "aion/gameserver/model/DuelResult.h"
#include "aion/gameserver/model/gameobjects/player/BlockList.h"
#include "aion/gameserver/model/gameobjects/player/BlockedPlayer.h"
#include "aion/gameserver/model/gameobjects/player/DeniedStatus.h"
#include "aion/gameserver/model/gameobjects/player/DeniedStatusInfo.h"
#include "aion/gameserver/model/gameobjects/player/Friend.h"
#include "aion/gameserver/model/gameobjects/player/FriendList.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/gameobjects/player/PlayerSettings.h"
#include "aion/gameserver/model/gameobjects/player/ResponseRequester.h"
#include "aion/gameserver/model/stats/container/PlayerLifeStats.h"
#include "aion/gameserver/network/aion/serverpackets/SM_BLOCK_LIST.h"
#include "aion/gameserver/network/aion/serverpackets/SM_BLOCK_RESPONSE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CLOSE_QUESTION_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DUEL.h"
#include "aion/gameserver/network/aion/serverpackets/SM_FRIEND_LIST.h"
#include "aion/gameserver/network/aion/serverpackets/SM_FRIEND_NOTIFY.h"
#include "aion/gameserver/network/aion/serverpackets/SM_FRIEND_RESPONSE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUESTION_WINDOW.h"
#include "aion/gameserver/services/DuelService.h"
#include "aion/gameserver/services/SocialService.h"
#include "aion/gameserver/services/player/PlayerReviveService.h"
#include "aion/gameserver/world/World.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing::team {
namespace {

using model::DuelResult;
using model::gameobjects::player::DeniedStatus;
using serverpackets::SM_BLOCK_LIST;
using serverpackets::SM_BLOCK_RESPONSE;
using serverpackets::SM_CLOSE_QUESTION_WINDOW;
using serverpackets::SM_DUEL;
using serverpackets::SM_FRIEND_LIST;
using serverpackets::SM_FRIEND_NOTIFY;
using serverpackets::SM_FRIEND_RESPONSE;
using serverpackets::SM_QUESTION_WINDOW;
using services::DuelService;
using services::SocialService;

constexpr int32_t ACCEPT_REQUEST = SM_QUESTION_WINDOW::STR_DUEL_DO_YOU_ACCEPT_REQUEST;
constexpr int32_t WITHDRAW_REQUEST = SM_QUESTION_WINDOW::STR_DUEL_DO_YOU_WITHDRAW_REQUEST;

class SocialDuelServiceTest : public TeamTest {
protected:
	void SocialDuelTearDown() {
		for (Member& m : members) {
			if (DuelService::getInstance().isDueling(m.player()))
				DuelService::getInstance().loseDuel(m.player()); // the singleton keeps its duels across the cases of one process
			world::World::getInstance().removeObject(*m.f.player);
		}
		TeamTest::TearDown();
	}

	/** addMember plus World.storeObject: loseDuel finds the winner and PlayerService.getPlayerName the name through the World */
	Member& addStoredMember(std::string_view name, float x = 100.0f) {
		Member& m = addMember(name, x);
		world::World::getInstance().storeObject(*m.f.player);
		return m;
	}

	/** Alpha asks Bravo; both questions are open */
	void request(Member& a, Member& b) {
		DuelService::getInstance().onDuelRequest(a.player(), runtime::Ptr<model::gameobjects::player::Player>(b.player()));
	}

	/** request + Bravo's yes: the duel runs */
	void startDuel(Member& a, Member& b) {
		request(a, b);
		ASSERT_TRUE(b.player().getResponseRequester().respond(ACCEPT_REQUEST, 1));
		clearAll();
	}

	/**
	 * The DAO test database, and the house lands SM_FRIEND_LIST reads (HousingService.findActiveHouse for each friend: HousingService's
	 * constructor reads the lands and the houses table)
	 */
	bool requireDatabase() {
		if (!dao::test::isEnabled())
			return false;
		dao::test::setUpDatabaseOnce();
		dao::test::clearTables();
		dataholders::DataManager::HOUSE_DATA.publish(xml::bindString<dataholders::HouseData>(houseContext, "<house_lands/>"));
		housesPublished = true;
		return true;
	}

	void TearDown() override {
		SocialDuelTearDown();
		if (housesPublished)
			dataholders::DataManager::HOUSE_DATA.resetForTests();
	}

	xml::LoadContext houseContext;
	bool housesPublished = false;
};

// ---- DuelService ----------------------------------------------------------------------------------------------------------------------------

/** DuelService.java:50-82: the refusals before any question */
TEST_F(SocialDuelServiceTest, ARequestIsRefusedWithoutAPartnerOrToADeadOrUnwillingOne) {
	Member& a = addStoredMember("Alpha");
	Member& b = addStoredMember("Bravo");
	DuelService& duels = DuelService::getInstance();
	duels.onDuelRequest(a.player(), nullptr);
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_DUEL_NO_USER_TO_REQUEST()), 1) << "no target";
	duels.onDuelRequest(a.player(), runtime::Ptr<model::gameobjects::player::Player>(a.player()));
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_DUEL_NO_USER_TO_REQUEST()), 2) << "himself";

	b.player().getPlayerSettings()->setDeny(model::gameobjects::player::getId(DeniedStatus::DUEL));
	request(a, b);
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_MSG_REJECTED_DUEL("Bravo")), 1) << "DeniedStatus.DUEL";
	b.player().getPlayerSettings()->setDeny(0);

	b.player().setLifeStats(std::make_unique<DeadPlayerLifeStats>(b.player()));
	request(a, b);
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_DUEL_PARTNER_INVALID("Bravo")), 1) << "a dead partner";
	EXPECT_EQ(b.count(SM_QUESTION_WINDOW_OPCODE), 0) << "nobody was asked";
}

/** DuelService.java:83-104, :114-130, :160-171: Bravo is asked, Alpha gets the withdraw question; Bravo's yes starts the duel for both */
TEST_F(SocialDuelServiceTest, TheAcceptedQuestionStartsTheDuel) {
	Member& a = addStoredMember("Alpha");
	Member& b = addStoredMember("Bravo");
	request(a, b);
	EXPECT_EQ(b.count(SM_QUESTION_WINDOW(ACCEPT_REQUEST, 0, 0, "Alpha")), 1);
	EXPECT_EQ(b.count(SM_SYSTEM_MESSAGE::STR_DUEL_REQUESTED("Alpha")), 1);
	EXPECT_EQ(a.count(SM_QUESTION_WINDOW(WITHDRAW_REQUEST, 0, 0, "Bravo")), 1);
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_DUEL_REQUEST_TO_PARTNER("Bravo")), 1);
	EXPECT_FALSE(DuelService::getInstance().isDueling(a.player()));

	clearAll();
	ASSERT_TRUE(b.player().getResponseRequester().respond(ACCEPT_REQUEST, 1));
	EXPECT_EQ(a.count(SM_CLOSE_QUESTION_WINDOW::CLOSE_QUESTION_WINDOW()), 1) << "Alpha's withdraw question is taken back";
	EXPECT_EQ(a.count(SM_DUEL::SM_DUEL_STARTED(b.player().getObjectId())), 1);
	EXPECT_EQ(b.count(SM_DUEL::SM_DUEL_STARTED(a.player().getObjectId())), 1);
	EXPECT_TRUE(DuelService::getInstance().isDueling(a.player(), b.player()));
	EXPECT_TRUE(DuelService::getInstance().isDueling(b.player(), a.player()));

	clearAll();
	request(a, b);
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_DUEL_YOU_ARE_IN_DUEL_ALREADY()), 1);
	Member& c = addStoredMember("Charlie");
	request(c, a);
	EXPECT_EQ(c.count(SM_SYSTEM_MESSAGE::STR_DUEL_PARTNER_IN_DUEL_ALREADY("Alpha")), 1);
}

/** DuelService.java:85-89, :140-144: Bravo's no closes Alpha's question and tells both */
TEST_F(SocialDuelServiceTest, ADeclinedQuestionTellsBoth) {
	Member& a = addStoredMember("Alpha");
	Member& b = addStoredMember("Bravo");
	request(a, b);
	clearAll();
	ASSERT_TRUE(b.player().getResponseRequester().respond(ACCEPT_REQUEST, 0));
	EXPECT_EQ(a.count(SM_CLOSE_QUESTION_WINDOW::STR_DUEL_HE_REJECT_DUEL("Bravo")), 1);
	EXPECT_EQ(b.count(SM_SYSTEM_MESSAGE::STR_DUEL_REJECT_DUEL("Alpha")), 1);
	EXPECT_FALSE(a.player().getResponseRequester().remove(WITHDRAW_REQUEST)) << "the withdraw question was removed";
	EXPECT_FALSE(DuelService::getInstance().isDueling(a.player()));
}

/** DuelService.java:121-125, :146-150: Alpha withdraws; Bravo's question closes */
TEST_F(SocialDuelServiceTest, AWithdrawnRequestClosesTheQuestion) {
	Member& a = addStoredMember("Alpha");
	Member& b = addStoredMember("Bravo");
	request(a, b);
	clearAll();
	ASSERT_TRUE(a.player().getResponseRequester().respond(WITHDRAW_REQUEST, 1));
	EXPECT_EQ(b.count(SM_CLOSE_QUESTION_WINDOW::STR_DUEL_REQUESTER_WITHDRAW_REQUEST("Alpha")), 1);
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_DUEL_WITHDRAW_REQUEST("Bravo")), 1);
	EXPECT_FALSE(b.player().getResponseRequester().remove(ACCEPT_REQUEST)) << "Bravo's question was removed";
}

/** DuelService.java:217-229, :231-241: five minutes after the start the duel ends in a draw for both */
TEST_F(SocialDuelServiceTest, FiveMinutesEndTheDuelInADraw) {
	Member& a = addStoredMember("Alpha");
	Member& b = addStoredMember("Bravo");
	startDuel(a, b);
	executor->advance(std::chrono::milliseconds(299'000));
	EXPECT_TRUE(DuelService::getInstance().isDueling(a.player()));
	executor->advance(std::chrono::milliseconds(1'500));
	EXPECT_FALSE(DuelService::getInstance().isDueling(a.player()));
	EXPECT_FALSE(DuelService::getInstance().isDueling(b.player()));
	EXPECT_EQ(a.count(SM_DUEL::SM_DUEL_RESULT(DuelResult::DUEL_DRAW, "Bravo")), 1);
	EXPECT_EQ(b.count(SM_DUEL::SM_DUEL_RESULT(DuelResult::DUEL_DRAW, "Alpha")), 1);
}

/** DuelService.java:189-198, :268-281: the loser loses, the winner wins, the draw task is cancelled */
TEST_F(SocialDuelServiceTest, ALostDuelEndsForBothAndCancelsTheDraw) {
	Member& a = addStoredMember("Alpha");
	Member& b = addStoredMember("Bravo");
	startDuel(a, b);
	DuelService::getInstance().loseDuel(b.player());
	EXPECT_EQ(b.count(SM_DUEL::SM_DUEL_RESULT(DuelResult::DUEL_LOST, "Alpha")), 1);
	EXPECT_EQ(a.count(SM_DUEL::SM_DUEL_RESULT(DuelResult::DUEL_WON, "Bravo")), 1);
	EXPECT_FALSE(DuelService::getInstance().isDueling(a.player()));
	clearAll();
	executor->advance(std::chrono::milliseconds(301'000));
	EXPECT_TRUE(a.sent().empty()) << "no draw after the end";
	EXPECT_TRUE(b.sent().empty());
	DuelService::getInstance().loseDuel(b.player()); // not dueling: nothing
	EXPECT_TRUE(b.sent().empty());
}

/** PlayerReviveService.java:36-40: a duel revive brings the loser back at 30 % */
TEST_F(SocialDuelServiceTest, ADuelReviveRestoresThirtyPercent) {
	Member& a = addStoredMember("Alpha");
	a.player().getLifeStats()->setCurrentHp(1);
	a.player().getLifeStats()->setCurrentMp(1);
	services::player::PlayerReviveService::duelRevive(a.player());
	// revive(player, 30, 30, false, 0): setCurrentHpPercent(30) / setCurrentMpPercent(30), maxHp * 30 / 100
	EXPECT_EQ(a.player().getLifeStats()->getCurrentHp(), static_cast<int32_t>(static_cast<int64_t>(a.player().getLifeStats()->getMaxHp()) * 30 / 100));
	EXPECT_EQ(a.player().getLifeStats()->getCurrentMp(), static_cast<int32_t>(static_cast<int64_t>(a.player().getLifeStats()->getMaxMp()) * 30 / 100));
	EXPECT_FALSE(a.player().isDead());
}

// ---- SocialService (the DAO test database) -------------------------------------------------------------------------------------------------

/** SocialService.java:82-97, :108-124: friends both ways, stored; a deletion tells the online friend */
TEST_F(SocialDuelServiceTest, FriendsAreMadeBothWaysAndDeletedBothWays) {
	if (!requireDatabase())
		GTEST_SKIP() << "set AION_TEST_GS_DATABASE_URL to run the database tests";
	Member& a = addStoredMember("Alpha");
	Member& b = addStoredMember("Bravo");
	dao::test::insertPlayer(a.player().getObjectId(), "Alpha", 9901);
	dao::test::insertPlayer(b.player().getObjectId(), "Bravo", 9902);

	ASSERT_TRUE(SocialService::makeFriends(a.player(), b.player()));
	ASSERT_NE(a.player().getFriendList().getFriend(b.player().getObjectId()), nullptr);
	ASSERT_NE(b.player().getFriendList().getFriend(a.player().getObjectId()), nullptr);
	EXPECT_EQ(a.count(SM_FRIEND_RESPONSE::TARGET_ADDED("Bravo")), 1);
	EXPECT_EQ(b.count(SM_FRIEND_RESPONSE::TARGET_ADDED("Alpha")), 1);
	EXPECT_EQ(dao::test::queryLong("SELECT COUNT(*) FROM friends").value_or(-1), 2) << "FriendListDAO.addFriends: both rows";
	EXPECT_FALSE(SocialService::makeFriends(a.player(), b.player())) << "already friends";

	clearAll();
	ASSERT_TRUE(SocialService::deleteFriend(a.player(), *a.player().getFriendList().getFriend(b.player().getObjectId())));
	EXPECT_EQ(a.player().getFriendList().getFriend(b.player().getObjectId()), nullptr);
	EXPECT_EQ(b.player().getFriendList().getFriend(a.player().getObjectId()), nullptr);
	EXPECT_EQ(b.count(SM_FRIEND_NOTIFY(SM_FRIEND_NOTIFY::DELETED, "Alpha")), 1);
	EXPECT_EQ(a.count(SM_FRIEND_RESPONSE::TARGET_REMOVED("Bravo")), 1);
	EXPECT_EQ(dao::test::queryLong("SELECT COUNT(*) FROM friends").value_or(-1), 0);
}

/** SocialService.java:66-75: a new memo is stored and sent, the same memo is not */
TEST_F(SocialDuelServiceTest, AFriendMemoChangesOnlyWhenItDiffers) {
	if (!requireDatabase())
		GTEST_SKIP() << "set AION_TEST_GS_DATABASE_URL to run the database tests";
	Member& a = addStoredMember("Alpha");
	Member& b = addStoredMember("Bravo");
	dao::test::insertPlayer(a.player().getObjectId(), "Alpha", 9901);
	dao::test::insertPlayer(b.player().getObjectId(), "Bravo", 9902);
	ASSERT_TRUE(SocialService::makeFriends(a.player(), b.player()));
	model::gameobjects::player::Friend& bravo = *a.player().getFriendList().getFriend(b.player().getObjectId());
	clearAll();
	EXPECT_FALSE(SocialService::setFriendMemo(a.player(), bravo, "")) << "the same (empty) memo";
	EXPECT_TRUE(a.sent().empty());
	EXPECT_TRUE(SocialService::setFriendMemo(a.player(), bravo, "tank"));
	EXPECT_EQ(bravo.getFriendMemo(), "tank");
	EXPECT_EQ(a.count(SM_FRIEND_LIST()), 1);
	EXPECT_EQ(dao::test::queryString("SELECT memo FROM friends WHERE player = " + std::to_string(a.player().getObjectId())).value_or(""), "tank");
}

/** SocialService.java:20-61: block, a new reason (the same one is no change), unblock - each stored and answered */
TEST_F(SocialDuelServiceTest, ABlockIsAddedEditedAndRemoved) {
	if (!requireDatabase())
		GTEST_SKIP() << "set AION_TEST_GS_DATABASE_URL to run the database tests";
	Member& a = addStoredMember("Alpha");
	Member& b = addStoredMember("Bravo");
	dao::test::insertPlayer(a.player().getObjectId(), "Alpha", 9901);
	dao::test::insertPlayer(b.player().getObjectId(), "Bravo", 9902);

	ASSERT_TRUE(SocialService::addBlockedUser(a.player(), *b.player().getCommonData(), "spam"));
	ASSERT_TRUE(a.player().getBlockList()->contains(b.player().getObjectId()));
	EXPECT_EQ(a.count(SM_BLOCK_LIST()), 1);
	EXPECT_EQ(a.count(SM_BLOCK_RESPONSE(SM_BLOCK_RESPONSE::BLOCK_SUCCESSFUL, "Bravo")), 1);
	EXPECT_EQ(dao::test::queryString("SELECT reason FROM blocks WHERE player = " + std::to_string(a.player().getObjectId())).value_or(""), "spam");

	model::gameobjects::player::BlockedPlayer& blocked = *a.player().getBlockList()->getBlockedPlayer(b.player().getObjectId());
	clearAll();
	EXPECT_FALSE(SocialService::setBlockedReason(a.player(), blocked, "spam")) << "the same reason";
	EXPECT_TRUE(a.sent().empty());
	EXPECT_TRUE(SocialService::setBlockedReason(a.player(), blocked, "rude"));
	EXPECT_EQ(a.count(SM_BLOCK_RESPONSE(SM_BLOCK_RESPONSE::EDIT_NOTE, "Bravo")), 1);
	EXPECT_EQ(dao::test::queryString("SELECT reason FROM blocks WHERE player = " + std::to_string(a.player().getObjectId())).value_or(""), "rude");

	clearAll();
	EXPECT_TRUE(SocialService::deleteBlockedUser(a.player(), blocked));
	EXPECT_FALSE(a.player().getBlockList()->contains(b.player().getObjectId()));
	EXPECT_EQ(a.count(SM_BLOCK_RESPONSE(SM_BLOCK_RESPONSE::UNBLOCK_SUCCESSFUL, "Bravo")), 1);
	EXPECT_EQ(dao::test::queryLong("SELECT COUNT(*) FROM blocks").value_or(-1), 0);
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing::team
