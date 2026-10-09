// M5h S-07 (P5-11): LegionService against LegionService.java - the restrictions' refusal order, create, invite (accept and deny), kick and leave,
// rank and permission changes, the level-up, the announcement and the emblem data chunks - on the test database aion_gs_test_legionhouse
// (LegionHouseTestSupport.h, run under gate_lock.py) with real Players on recording connections stored in the World
// (tests/team/P5-10b/TeamTestSupport.h, included by relative path as tests/team includes tests/cm_ak).

#include "../team/P5-10b/TeamTestSupport.h"
#include "LegionHouseTestSupport.h"

#include <regex>
#include <string>
#include <unordered_map>
#include <vector>

#include <gtest/gtest.h>

#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/dataholders/HouseData.bind.h"
#include "aion/gameserver/dataholders/HouseData.h"
#include "aion/gameserver/dataholders/LegionDominionData.bind.h"
#include "aion/gameserver/dataholders/LegionDominionData.h"
#include "aion/gameserver/model/legionDominion/LegionDominionLocation.h"
#include "aion/gameserver/model/legionDominion/LegionDominionParticipantInfo.h"
#include "aion/gameserver/model/templates/LegionDominionReward.h"
#include "aion/gameserver/network/aion/serverpackets/SM_LEGION_DOMINION_LOC_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_LEGION_DOMINION_RANK.h"
#include "aion/gameserver/services/LegionDominionService.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/main/LegionConfig.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/items/storage/StorageType.h"
#include "aion/gameserver/model/items/storage/StorageTypeInfo.h"
#include "aion/gameserver/model/team/legion/Legion.h"
#include "aion/gameserver/model/team/legion/LegionEmblem.h"
#include "aion/gameserver/model/team/legion/LegionMember.h"
#include "aion/gameserver/model/gameobjects/player/ResponseRequester.h"
#include "aion/gameserver/network/aion/serverpackets/SM_LEGION_ADD_MEMBER.h"
#include "aion/gameserver/network/aion/serverpackets/SM_LEGION_EDIT.h"
#include "aion/gameserver/network/aion/serverpackets/SM_LEGION_HISTORY.h"
#include "aion/gameserver/network/aion/serverpackets/SM_LEGION_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_LEGION_LEAVE_MEMBER.h"
#include "aion/gameserver/network/aion/serverpackets/SM_LEGION_MEMBERLIST.h"
#include "aion/gameserver/network/aion/serverpackets/SM_LEGION_SEND_EMBLEM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_LEGION_SEND_EMBLEM_DATA.h"
#include "aion/gameserver/network/aion/serverpackets/SM_LEGION_UPDATE_MEMBER.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUESTION_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_RENAME.h"
#include "aion/gameserver/services/LegionService.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"
#include "aion/gameserver/world/World.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing::team {
namespace {

namespace lh = ::aion::gameserver::legionhouse::test;
using configs::main::LegionConfig;
using model::team::legion::Legion;
using model::team::legion::LegionMember;
using model::team::legion::LegionRank;
using serverpackets::SM_QUESTION_WINDOW;
using services::LegionService;

class LegionServiceTest : public TeamTest {
protected:
	void SetUp() override {
		TeamTest::SetUp();
		utils::idfactory::IDFactory::getInstance().resetForTests();
		LegionConfig::LEGION_NAME_PATTERN.set(std::wregex(L"[a-zA-Z ]{2,32}"));
		LegionConfig::SELF_INTRO_PATTERN.set(std::wregex(L".{1,32}"));
		LegionConfig::NICKNAME_PATTERN.set(std::wregex(L".{1,10}"));
	}

	void TearDown() override {
		for (Member* m : stored) {
			m->player().setLegionMember(nullptr);
			world::World::getInstance().removeObject(m->player());
		}
		stored.clear();
		TeamTest::TearDown();
	}

	bool requireDatabase() {
		if (!lh::isDatabaseEnabled())
			return false;
		lh::setUpDatabaseOnce();
		return true;
	}

	/** An online player (spawned, on a recording connection, in the World) with a players row and `kinah` in the cube */
	Member& online(std::string_view name, int64_t kinah = 0) {
		Member& m = addMember(name);
		world::World::getInstance().storeObject(m.player());
		stored.push_back(&m);
		lh::insertPlayer(m.player().getObjectId(), name, m.player().getObjectId());
		if (kinah > 0)
			giveKinah(m, 900000 + static_cast<int32_t>(stored.size()), kinah);
		m.clearSent();
		return m;
	}

	/** A creates the legion `name` (10,000 kinah) */
	Legion& created(Member& a, std::string_view name = "Founders") {
		LegionService::getInstance().createLegion(a.player(), name);
		EXPECT_TRUE(a.player().getLegion()) << "the legion was not created";
		return *a.player().getLegion();
	}

	/** `target` is invited by `inviter` and answers `response` (1 accepts) */
	void invite(Member& inviter, Member& target, int32_t response) {
		LegionService::getInstance().invitePlayerToLegion(inviter.player(), target.player().getName());
		target.player().getResponseRequester().respond(SM_QUESTION_WINDOW::STR_GUILD_INVITE_DO_YOU_ACCEPT_INVITATION, response);
	}

	static int64_t count(std::string_view sql) { return *lh::queryLong(sql); }

	std::vector<Member*> stored;
	// the member list and the member packets read the members' houses (none here)
	lh::PublishedHolder<dataholders::HouseData> houseData{dataholders::DataManager::HOUSE_DATA, lh::bindXml<dataholders::HouseData>("<house_lands/>")};
	ConfigScope<int32_t> createKinah{LegionConfig::LEGION_CREATE_REQUIRED_KINAH, 10000};
	ConfigScope<int32_t> maxMembers{LegionConfig::LEGION_LEVEL1_MAX_MEMBERS, 30};
	ConfigScope<bool> cp{configs::main::CustomConfig::CONQUEROR_AND_PROTECTOR_SYSTEM_ENABLED, false};
};

#define LEGION_REQUIRE_DATABASE()                                                                                                                     \
	if (!requireDatabase())                                                                                                                           \
		GTEST_SKIP() << "set AION_TEST_GS_DATABASE_URL to run the database tests";

TEST_F(LegionServiceTest, CreateRefusesInJavasOrderNameFreeNameMembershipKinah) {
	LEGION_REQUIRE_DATABASE();
	Member& a = online("Alpha", 5000);
	LegionService& service = LegionService::getInstance();
	service.createLegion(a.player(), "x1"); // the pattern refuses the digit and the length
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_GUILD_CREATE_INVALID_GUILD_NAME()), 1);
	lh::execute("INSERT INTO legions (id, name) VALUES (777, 'Taken')");
	service.createLegion(a.player(), "Taken");
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_GUILD_CREATE_SAME_GUILD_EXIST()), 1) << "the name is checked before the kinah";
	service.createLegion(a.player(), "Poor");
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_GUILD_CREATE_NOT_ENOUGH_MONEY()), 1) << "5,000 of 10,000";
	EXPECT_FALSE(a.player().getLegion());
	EXPECT_EQ(count("SELECT COUNT(*) FROM legions"), 1);
}

TEST_F(LegionServiceTest, CreateMakesTheCreatorBrigadeGeneralTakesTheKinahAndWritesTwoHistoryEntries) {
	LEGION_REQUIRE_DATABASE();
	Member& a = online("Alpha", 25000);
	Legion& legion = created(a);
	EXPECT_EQ(a.player().getLegionMember()->getRank(), LegionRank::BRIGADE_GENERAL);
	EXPECT_EQ(a.player().getInventory().getKinah(), 15000);
	EXPECT_EQ(count("SELECT COUNT(*) FROM legions WHERE name = 'Founders' AND id = " + std::to_string(legion.getLegionId())), 1);
	EXPECT_EQ(count("SELECT COUNT(*) FROM legion_members WHERE `rank` = 'BRIGADE_GENERAL' AND player_id = " + std::to_string(a.player().getObjectId())), 1);
	EXPECT_EQ(count("SELECT COUNT(*) FROM legion_history WHERE history_type IN ('CREATE', 'JOIN')"), 2);
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_GUILD_CREATED("Founders")), 1);
	EXPECT_EQ(a.count(opcodeOf<serverpackets::SM_LEGION_INFO>), 1);
	EXPECT_EQ(legion.getHistory(model::team::legion::LegionHistoryAction_Type::LEGION).size(), 2u);
	LegionService::getInstance().createLegion(a.player(), "Second");
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_GUILD_CREATE_ALREADY_BELONGS_TO_GUILD()), 1) << "membership is checked before the kinah";
}

TEST_F(LegionServiceTest, AnInviteAcceptedAddsAVolunteerAndADeniedOneTellsTheInviter) {
	LEGION_REQUIRE_DATABASE();
	Member& a = online("Alpha", 25000);
	Member& b = online("Bravo");
	Member& c = online("Charlie");
	Legion& legion = created(a);
	a.clearSent();
	invite(a, b, 1);
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_GUILD_INVITE_SENT_INVITE_MSG_TO_HIM("Bravo")), 1);
	EXPECT_EQ(b.count(opcodeOf<serverpackets::SM_QUESTION_WINDOW>), 1);
	ASSERT_TRUE(b.player().getLegionMember());
	EXPECT_EQ(b.player().getLegionMember()->getRank(), LegionRank::VOLUNTEER);
	EXPECT_TRUE(legion.isMember(b.player().getObjectId()));
	EXPECT_EQ(a.count(opcodeOf<serverpackets::SM_LEGION_ADD_MEMBER>), 1) << "the members are told of the new member";
	EXPECT_EQ(count("SELECT COUNT(*) FROM legion_history WHERE history_type = 'JOIN' AND name = 'Bravo'"), 1);
	invite(a, c, 0);
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_GUILD_INVITE_HE_REJECTED_INVITATION("Charlie")), 1);
	EXPECT_FALSE(c.player().getLegionMember());
	// the refusals: a member of the own legion, and a volunteer has no invite right
	invite(a, b, 1);
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_GUILD_INVITE_HE_IS_MY_GUILD_MEMBER("Bravo")), 1);
	LegionService::getInstance().invitePlayerToLegion(b.player(), "Charlie");
	EXPECT_EQ(b.count(SM_SYSTEM_MESSAGE::STR_GUILD_INVITE_DONT_HAVE_RIGHT_TO_INVITE()), 1);
	LegionService::getInstance().invitePlayerToLegion(a.player(), "Nobody");
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_GUILD_INVITE_NO_USER_TO_INVITE()), 1);
}

TEST_F(LegionServiceTest, AKickedOnlineMemberIsToldAndRemovedAndAMemberLeavesByHimself) {
	LEGION_REQUIRE_DATABASE();
	Member& a = online("Alpha", 25000);
	Member& b = online("Bravo");
	Member& c = online("Charlie");
	Legion& legion = created(a);
	invite(a, b, 1);
	invite(a, c, 1);
	a.clearSent();
	b.clearSent();
	LegionService::getInstance().kickMember(b.player(), "Charlie");
	EXPECT_EQ(b.count(SM_SYSTEM_MESSAGE::STR_GUILD_BANISH_CAN_NOT_BANISH_SAME_MEMBER_RANK()), 1) << "a volunteer kicks no volunteer";
	LegionService::getInstance().kickMember(a.player(), "Alpha");
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_GUILD_BANISH_CANT_BANISH_SELF()), 1);
	LegionService::getInstance().kickMember(a.player(), "Bravo");
	EXPECT_FALSE(b.player().getLegionMember());
	EXPECT_FALSE(legion.isMember(b.player().getObjectId()));
	EXPECT_EQ(b.count(serverpackets::SM_LEGION_LEAVE_MEMBER(1300246, 0, "Founders")), 1);
	EXPECT_EQ(a.count(opcodeOf<serverpackets::SM_LEGION_LEAVE_MEMBER>), 1) << "1300247 to the others";
	EXPECT_EQ(count("SELECT COUNT(*) FROM legion_members WHERE player_id = " + std::to_string(b.player().getObjectId())), 0);
	EXPECT_EQ(count("SELECT COUNT(*) FROM legion_history WHERE history_type = 'KICK' AND name = 'Bravo'"), 1);
	EXPECT_FALSE(LegionService::getInstance().leaveLegion(a.player(), false)) << "the brigade general cannot leave";
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_GUILD_LEAVE_MASTER_CANT_LEAVE_BEFORE_CHANGE_MASTER()), 1);
	EXPECT_TRUE(LegionService::getInstance().leaveLegion(c.player(), false));
	EXPECT_EQ(c.count(serverpackets::SM_LEGION_LEAVE_MEMBER(1300241, 0, "Founders")), 1);
	EXPECT_EQ(legion.getMemberIds()->size(), 1);
}

TEST_F(LegionServiceTest, RanksPermissionsAndTheBrigadeGeneralTransfer) {
	LEGION_REQUIRE_DATABASE();
	Member& a = online("Alpha", 25000);
	Member& b = online("Bravo");
	Legion& legion = created(a);
	invite(a, b, 1);
	b.clearSent();
	LegionService::getInstance().appointRank(a.player(), "Bravo", 2);
	EXPECT_EQ(b.player().getLegionMember()->getRank(), LegionRank::CENTURION);
	EXPECT_EQ(b.count(serverpackets::SM_LEGION_UPDATE_MEMBER(*b.player().getLegionMember(), 1300267, "Bravo")), 1);
	LegionService::getInstance().changePermissions(b.player(), 1, 2, 3, 4);
	EXPECT_EQ(b.count(SM_SYSTEM_MESSAGE::STR_GUILD_CHANGE_RIGHT_DONT_HAVE_RIGHT()), 1);
	LegionService::getInstance().changePermissions(a.player(), 1, 2, 3, 4);
	EXPECT_EQ(legion.getDeputyPermission(), 1);
	EXPECT_EQ(legion.getVolunteerPermission(), 4);
	LegionService::getInstance().appointBrigadeGeneral(*b.player().getLegionMember());
	EXPECT_EQ(b.player().getLegionMember()->getRank(), LegionRank::BRIGADE_GENERAL);
	EXPECT_EQ(a.player().getLegionMember()->getRank(), LegionRank::CENTURION) << "the previous brigade general becomes a centurion";
	EXPECT_EQ(count("SELECT COUNT(*) FROM legion_history WHERE history_type = 'APPOINTED' AND name = 'Bravo'"), 1);
	EXPECT_THROW(LegionService::getInstance().appointRank(b.player(), "Alpha", 5), runtime::ArrayIndexOutOfBoundsException)
		<< "LegionRank.values()[5]";
}

TEST_F(LegionServiceTest, TheLevelUpChecksMembersKinahAndPoints) {
	LEGION_REQUIRE_DATABASE();
	ConfigScope<int32_t> requiredMembers(LegionConfig::LEGION_LEVEL2_REQUIRED_MEMBERS, 1);
	ConfigScope<int32_t> kinah(LegionConfig::LEGION_LEVEL2_REQUIRED_KINAH, 5000);
	ConfigScope<int32_t> points(LegionConfig::LEGION_LEVEL2_REQUIRED_CONTRIBUTION, 10);
	ConfigScope<bool> tasks(LegionConfig::ENABLE_GUILD_TASK_REQ, false);
	Member& a = online("Alpha", 20000);
	Legion& legion = created(a);
	LegionService::getInstance().requestChangeLevel(a.player());
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_GUILD_CHANGE_LEVEL_NOT_ENOUGH_POINT()), 1);
	EXPECT_EQ(legion.getLegionLevel(), 1);
	legion.addContributionPoints(10);
	a.clearSent();
	LegionService::getInstance().requestChangeLevel(a.player());
	EXPECT_EQ(legion.getLegionLevel(), 2);
	EXPECT_EQ(a.player().getInventory().getKinah(), 5000) << "20,000 - 10,000 (creation) - 5,000 (level 2)";
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_GUILD_EVENT_LEVELUP(2)), 1);
	EXPECT_EQ(count("SELECT COUNT(*) FROM legion_history WHERE history_type = 'LEVEL_UP' AND name = '2'"), 1);
}

TEST_F(LegionServiceTest, TheAnnouncementIsTruncatedAt256AndAnEmptyOneClearsIt) {
	LEGION_REQUIRE_DATABASE();
	Member& a = online("Alpha", 25000);
	Member& b = online("Bravo");
	Legion& legion = created(a);
	invite(a, b, 1);
	LegionService::getInstance().changeAnnouncement(b.player(), "hello");
	EXPECT_EQ(b.count(SM_SYSTEM_MESSAGE::STR_GUILD_WRITE_NOTICE_DONT_HAVE_RIGHT()), 1) << "a volunteer has no EDIT right";
	a.clearSent();
	LegionService::getInstance().changeAnnouncement(a.player(), std::string(300, 'n'));
	ASSERT_TRUE(legion.getAnnouncement());
	EXPECT_EQ(legion.getAnnouncement()->message(), std::string(256, 'n'));
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_GUILD_WRITE_NOTICE_DONE()), 1);
	EXPECT_EQ(count("SELECT COUNT(*) FROM legion_announcement_list WHERE CHAR_LENGTH(announcement) = 256"), 1);
	LegionService::getInstance().changeAnnouncement(a.player(), "");
	EXPECT_FALSE(legion.getAnnouncement());
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_MSG_CLEAR_GUILD_NOTICE()), 1);
	EXPECT_EQ(b.count(opcodeOf<serverpackets::SM_LEGION_INFO>), 2) << "his own on joining, and the cleared one (not to A)";
	EXPECT_EQ(a.count(opcodeOf<serverpackets::SM_LEGION_INFO>), 0) << "the clearing member is excluded from the broadcast";
}

TEST_F(LegionServiceTest, EmblemDataIsSentIn7993ByteChunks) {
	LEGION_REQUIRE_DATABASE();
	Member& a = online("Alpha");
	runtime::Ref<model::team::legion::LegionEmblem> emblem = model::team::legion::LegionEmblem::create();
	const std::array<std::pair<int32_t, int32_t>, 5> cases{{{0, 0}, {1, 1}, {7993, 1}, {7994, 2}, {15987, 3}}};
	for (const auto& [length, chunks] : cases) {
		emblem->setCustomEmblemData(runtime::Array<int8_t>::make(length));
		a.clearSent();
		LegionService::getInstance().sendEmblemData(a.player(), *emblem, 5, "Emblems");
		EXPECT_EQ(a.count(opcodeOf<serverpackets::SM_LEGION_SEND_EMBLEM>), 1) << length;
		EXPECT_EQ(a.count(opcodeOf<serverpackets::SM_LEGION_SEND_EMBLEM_DATA>), chunks) << length << " bytes";
	}
}

TEST_F(LegionServiceTest, AMembersLegionWarehouseProxyIsCreatedForThePublishedPlayer) {
	LEGION_REQUIRE_DATABASE();
	// the M5h gate's C8 (2026-10-08): the logout's store asked a published legion member for his LEGION_WAREHOUSE storage, whose proxy bound
	// its owner after publication (AION_CHECKED C11); the proxy now takes its owner at construction
	Member& a = online("Alpha", 25000);
	Legion& legion = created(a);
	const int32_t legionWarehouse = model::items::storage::getId(model::items::storage::StorageType::LEGION_WAREHOUSE);
	runtime::Ptr<model::items::storage::Storage> first = a.player().getStorage(legionWarehouse);
	ASSERT_TRUE(first);
	EXPECT_EQ(first->getStorageType(), model::items::storage::StorageType::LEGION_WAREHOUSE);
	runtime::Ptr<model::items::storage::Storage> second = a.player().getStorage(legionWarehouse);
	EXPECT_TRUE(second) << "a second proxy replaces the first in the slot";
	static_cast<void>(legion);
}

TEST_F(LegionServiceTest, AnExpiredDisbandTimeDisbandsTheLegionOnTheNextLookup) {
	LEGION_REQUIRE_DATABASE();
	Member& a = online("Alpha", 25000);
	Legion& legion = created(a);
	const int32_t legionId = legion.getLegionId();
	legion.setDisbandTime(1); // long past: checkDisband disbands (cleanLegionId over no siege location, the members told)
	a.clearSent();
	EXPECT_FALSE(LegionService::getInstance().getLegion(legionId));
	EXPECT_EQ(a.count(serverpackets::SM_LEGION_LEAVE_MEMBER(1300302, 0, "Founders")), 1);
	EXPECT_FALSE(a.player().getLegionMember());
	EXPECT_EQ(count("SELECT COUNT(*) FROM legions WHERE id = " + std::to_string(legionId)), 0);
}

// tryRename (LegionService.java:1119-1146), the legion-name-change ticket of CM_APPEARANCE: the refusals in Java's order, the ticket check, the rename
TEST_F(LegionServiceTest, TryRenameRefusesInJavasOrderThenRenamesStoresAndTellsTheWorld) {
	LEGION_REQUIRE_DATABASE();
	Member& a = online("Alpha", 25000);
	Legion& legion = created(a);
	LegionService& service = LegionService::getInstance();
	lh::execute("INSERT INTO legions (id, name) VALUES (777, 'Taken')");
	a.clearSent();
	EXPECT_FALSE(service.tryRename(legion, "Founders", a.player(), std::nullopt));
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_MSG_EDIT_GUILD_NAME_ERROR_SAME_YOUR_NAME()), 1);
	EXPECT_FALSE(service.tryRename(legion, "x1", a.player(), std::nullopt));
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_MSG_EDIT_GUILD_NAME_ERROR_WRONG_INPUT()), 1) << "isValidLegionName";
	EXPECT_FALSE(service.tryRename(legion, "Taken", a.player(), std::nullopt));
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_MSG_EDIT_GUILD_NAME_ALREADY_EXIST()), 1) << "LegionDAO.isNameUsed";
	legion.setDisbandTime(static_cast<int32_t>(commons::utils::currentTimeMillis() / 1000) + 3600);
	EXPECT_FALSE(service.tryRename(legion, "Renamed", a.player(), std::nullopt));
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_MSG_EDIT_GUILD_NAME_CANT_FOR_DISPERSING_GUILD()), 1);
	legion.setDisbandTime(0);
	EXPECT_FALSE(service.tryRename(legion, "Renamed", a.player(), 4242)) << "no ticket with that object id: AuditLogger, refused";
	runtime::Ref<model::gameobjects::Item> juice = items::loadedItem(910001, items::MERCENARYS_FRUIT_JUICE, 5, model::items::storage::StorageType::CUBE);
	a.player().getStorage(model::items::storage::getId(model::items::storage::StorageType::CUBE))->onLoadHandler(*juice);
	EXPECT_FALSE(service.tryRename(legion, "Renamed", a.player(), 910001)) << "an item that is no ticket 169680000/169680001";
	EXPECT_EQ(a.player().getInventory().getItemCountByItemId(items::MERCENARYS_FRUIT_JUICE), 5) << "nothing taken from it";
	EXPECT_EQ(legion.getName(), "Founders");
	EXPECT_EQ(a.count(opcodeOf<serverpackets::SM_RENAME>), 0);

	EXPECT_TRUE(service.tryRename(legion, "Renamed", a.player(), std::nullopt));
	EXPECT_EQ(legion.getName(), "Renamed");
	EXPECT_EQ(count("SELECT COUNT(*) FROM legions WHERE name = 'Renamed' AND id = " + std::to_string(legion.getLegionId())), 1) << "LegionDAO.storeLegion";
	EXPECT_EQ(count("SELECT COUNT(*) FROM legion_history WHERE history_type = 'LEGION_RENAME' AND name = 'Founders' AND description = 'Renamed'"), 1)
	  << "addHistory(legion, oldName, LEGION_RENAME, name)";
	EXPECT_EQ(a.count(serverpackets::SM_RENAME(legion, "Founders")), 1) << "broadcastToWorld";
}

// L-05: LegionDAO.loadHistory files each row under its action's type through LegionHistoryActionInfo.h's getType (LegionHistoryAction.java)
TEST_F(LegionServiceTest, ALoadedLegionsHistoryIsFiledByTheActionsType) {
	LEGION_REQUIRE_DATABASE();
	lh::execute("INSERT INTO legions (id, name) VALUES (778, 'Keepers')");
	lh::execute("INSERT INTO legion_history (legion_id, history_type, name, description) VALUES (778, 'JOIN', 'A', ''), (778, 'DEFENSE', 'B', ''), "
	            "(778, 'OCCUPATION', 'C', ''), (778, 'KINAH_DEPOSIT', 'D', '1000'), (778, 'ITEM_WITHDRAW', 'E', ''), (778, 'LEGION_RENAME', 'F', 'G')");
	runtime::Ptr<Legion> legion = LegionService::getInstance().getLegion(778);
	ASSERT_TRUE(legion);
	using model::team::legion::LegionHistoryAction_Type;
	EXPECT_EQ(legion->getHistory(LegionHistoryAction_Type::LEGION).size(), 2u) << "JOIN, LEGION_RENAME";
	EXPECT_EQ(legion->getHistory(LegionHistoryAction_Type::REWARD).size(), 2u) << "DEFENSE, OCCUPATION";
	EXPECT_EQ(legion->getHistory(LegionHistoryAction_Type::WAREHOUSE).size(), 2u) << "KINAH_DEPOSIT, ITEM_WITHDRAW";
}

// ---- S-08: the Legion Dominion weekly calculation (LegionDominionService.java:84-164, LegionDominionLocation.java:83-96, 129-131) ----

/** One territory with the shipped rewards' shape (legion_dominion_template.xml, location 1): three of rank 1, one each of ranks 2 and 3 */
constexpr const char* ONE_DOMINION = R"(<legion_dominion_template>
	<legion_dominion_location id="1" world_id="220080000" zone="LegionDominionArea_01" race="ASMODIANS" name_id="404623">
		<reward rank="1" item_id="188053896" count="1" />
		<reward rank="1" item_id="185000233" count="1" />
		<reward rank="1" item_id="169610352" count="1" />
		<reward rank="2" item_id="188053896" count="1" />
		<reward rank="3" item_id="188053897" count="1" />
	</legion_dominion_location>
</legion_dominion_template>)";

class LegionDominionWeeklyTest : public LegionServiceTest {
protected:
	/** the participant row of `legion` in territory 1 */
	static void participate(Legion& legion, int32_t points, std::string_view date) {
		lh::execute("INSERT INTO legion_dominion_participants (legion_dominion_id, legion_id, points, survived_time, participated_date) VALUES (1, " +
			std::to_string(legion.getLegionId()) + ", " + std::to_string(points) + ", 0, '" + std::string(date) + "')");
	}

	/** publishes the territory and runs LegionDominionService.initLocations (the location row is created, the participants loaded) */
	services::LegionDominionService& service() {
		services::LegionDominionService::getInstance().initLocations();
		return services::LegionDominionService::getInstance();
	}

	lh::PublishedHolder<dataholders::LegionDominionData> dominions{dataholders::DataManager::LEGION_DOMINION_DATA,
		lh::bindXml<dataholders::LegionDominionData>(ONE_DOMINION)};
	ConfigScope<int32_t> minPoints{LegionConfig::STONESPEAR_REACH_MIN_POINTS_FOR_TERRITORY, 50};
};

TEST_F(LegionDominionWeeklyTest, WithoutParticipantsTheTerritoryIsResetAndTheWorldTold) {
	LEGION_REQUIRE_DATABASE();
	Member& a = online("Alpha");
	service().startWeeklyCalculation();
	EXPECT_EQ(services::LegionDominionService::getInstance().getLegionDominionLoc(1)->getLegionId(), 0);
	EXPECT_EQ(count("SELECT legion_id FROM legion_dominion_locations WHERE id = 1"), 0);
	EXPECT_EQ(a.count(opcodeOf<serverpackets::SM_LEGION_DOMINION_LOC_INFO>), 1) << "one world broadcast";
}

TEST_F(LegionDominionWeeklyTest, TheBestEligibleLegionOccupiesThePreviousOccupierIsClearedAndTheParticipantsAreDeleted) {
	LEGION_REQUIRE_DATABASE();
	Member& a = online("Alpha", 25000);
	Member& b = online("Bravo", 25000);
	Member& c = online("Charlie", 25000);
	Member& d = online("Delta", 25000);
	Legion& winner = created(a, "Winners");
	Legion& holder = created(b, "Holders");
	Legion& late = created(c, "Latecomers");
	Legion& fewer = created(d, "Fewer");
	lh::execute("INSERT INTO legion_dominion_locations (id, legion_id) VALUES (1, " + std::to_string(holder.getLegionId()) + ")");
	holder.setOccupiedLegionDominion(1);
	participate(winner, 100, "2026-10-02 10:00:00");
	participate(late, 100, "2026-10-03 10:00:00"); // the same points, later: second
	participate(fewer, 90, "2026-10-01 10:00:00"); // the earliest, but fewer points: third
	participate(holder, 50, "2026-10-01 09:00:00"); // not above the minimum: not ranked
	services::LegionDominionService& weekly = service();
	ASSERT_EQ(weekly.getLegionDominionLoc(1)->getLegionRanking(true).size(), 3u);
	EXPECT_EQ(weekly.getLegionDominionLoc(1)->getLegionRanking(true)[0]->getLegionId(), winner.getLegionId()) << "points desc, then the earlier date";
	EXPECT_EQ(weekly.getLegionDominionLoc(1)->getLegionRanking(true)[1]->getLegionId(), late.getLegionId());
	EXPECT_EQ(weekly.getLegionDominionLoc(1)->getLegionRanking(true)[2]->getLegionId(), fewer.getLegionId());
	EXPECT_EQ(weekly.getLegionDominionLoc(1)->getLegionRanking(false).size(), 4u);
	for (Member* m : stored)
		m->clearSent();
	weekly.startWeeklyCalculation();
	EXPECT_EQ(weekly.getLegionDominionLoc(1)->getLegionId(), winner.getLegionId());
	EXPECT_EQ(winner.getOccupiedLegionDominion(), 1);
	EXPECT_EQ(winner.getLastLegionDominion(), 1);
	EXPECT_EQ(late.getOccupiedLegionDominion(), 0);
	EXPECT_EQ(late.getLastLegionDominion(), 1);
	EXPECT_EQ(holder.getOccupiedLegionDominion(), 0) << "the previous occupier is cleared";
	EXPECT_EQ(count("SELECT occupied_legion_dominion FROM legions WHERE id = " + std::to_string(winner.getLegionId())), 1);
	EXPECT_EQ(count("SELECT occupied_legion_dominion FROM legions WHERE id = " + std::to_string(holder.getLegionId())), 0);
	EXPECT_EQ(count("SELECT legion_id FROM legion_dominion_locations WHERE id = 1"), winner.getLegionId());
	EXPECT_EQ(count("SELECT COUNT(*) FROM legion_dominion_participants"), 0);
	EXPECT_TRUE(weekly.getLegionDominionLoc(1)->getParticipantInfo()->isEmpty()) << "reset";
	EXPECT_EQ(a.count(opcodeOf<serverpackets::SM_LEGION_DOMINION_RANK>), 1);
	EXPECT_EQ(b.count(opcodeOf<serverpackets::SM_LEGION_DOMINION_RANK>), 1) << "once: the previous occupier is skipped in the participant loop";
	EXPECT_EQ(c.count(opcodeOf<serverpackets::SM_LEGION_DOMINION_RANK>), 1);
	EXPECT_EQ(d.count(opcodeOf<serverpackets::SM_LEGION_DOMINION_RANK>), 1);
	EXPECT_EQ(a.count(opcodeOf<serverpackets::SM_LEGION_DOMINION_LOC_INFO>), 1);
}

TEST_F(LegionDominionWeeklyTest, ADisbandingWinnerDoesNotOccupy) {
	LEGION_REQUIRE_DATABASE();
	Member& a = online("Alpha", 25000);
	Legion& winner = created(a, "Leaving");
	participate(winner, 100, "2026-10-01 10:00:00");
	services::LegionDominionService& weekly = service();
	winner.setDisbandTime(static_cast<int32_t>(commons::utils::currentTimeMillis() / 1000) + 3600);
	weekly.startWeeklyCalculation();
	EXPECT_EQ(weekly.getLegionDominionLoc(1)->getLegionId(), 0);
	EXPECT_EQ(winner.getOccupiedLegionDominion(), 0);
	EXPECT_EQ(winner.getLastLegionDominion(), 1) << "still updated as a participant";
}

TEST_F(LegionDominionWeeklyTest, TheRewardsAreGroupedByRank) {
	LEGION_REQUIRE_DATABASE();
	service();
	std::unordered_map<int32_t, std::vector<const model::templates::LegionDominionReward*>> rewards =
		services::LegionDominionService::getInstance().getLegionDominionLoc(1)->getRewards();
	ASSERT_EQ(rewards.size(), 3u);
	ASSERT_EQ(rewards[1].size(), 3u);
	EXPECT_EQ(rewards[1][0]->getItemId(), 188053896);
	EXPECT_EQ(rewards[1][2]->getItemId(), 169610352) << "the template order";
	EXPECT_EQ(rewards[3][0]->getItemId(), 188053897);
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing::team
