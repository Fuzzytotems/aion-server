// P5-10f (M5h L-06): the legion model - Legion, LegionMember, LegionEmblem, LegionWarehouse and the four enum companions - against the Java
// bodies (Legion.java, LegionMember.java, LegionEmblem.java, LegionWarehouse.java, the four enums). Real Players on recording connections
// (tests/team/P5-10b/TeamTestSupport.h) are stored in the World for getOnlinePlayers and the legion bonus.

#include "../P5-10b/TeamTestSupport.h"

#include <array>
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/main/LegionConfig.h"
#include "aion/gameserver/model/team/legion/Legion.h"
#include "aion/gameserver/model/team/legion/LegionEmblem.h"
#include "aion/gameserver/model/team/legion/LegionEmblemTypeInfo.h"
#include "aion/gameserver/model/team/legion/LegionHistoryActionInfo.h"
#include "aion/gameserver/model/team/legion/LegionHistoryEntry.h"
#include "aion/gameserver/model/team/legion/LegionMember.h"
#include "aion/gameserver/model/team/legion/LegionPermissionsMaskInfo.h"
#include "aion/gameserver/model/team/legion/LegionRankInfo.h"
#include "aion/gameserver/model/team/legion/LegionWarehouse.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ICON_INFO.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/world/World.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing::team {
namespace {

using configs::main::LegionConfig;
using model::gameobjects::Persistable;
using model::team::legion::Legion;
using model::team::legion::LegionEmblem;
using model::team::legion::LegionEmblemType;
using model::team::legion::LegionHistoryAction;
using model::team::legion::LegionHistoryAction_Type;
using model::team::legion::LegionHistoryEntry;
using model::team::legion::LegionMember;
using model::team::legion::LegionPermissionsMask;
using model::team::legion::LegionRank;

constexpr std::array<LegionPermissionsMask, 7> MASKS{LegionPermissionsMask::EDIT, LegionPermissionsMask::INVITE, LegionPermissionsMask::KICK,
	LegionPermissionsMask::WH_WITHDRAWAL, LegionPermissionsMask::WH_DEPOSIT, LegionPermissionsMask::ARTIFACT, LegionPermissionsMask::GUARDIAN_STONE};

// ---- the companions against the Java constructor arguments ----

TEST(LegionCompanionsTest, PermissionMaskBitsAreTheJavaConstructorArguments) {
	// LegionPermissionsMask.java:8-14: EDIT(0x200), INVITE(0x8), KICK(0x10), WH_WITHDRAWAL(0x4), WH_DEPOSIT(0x1000), ARTIFACT(0x400),
	// GUARDIAN_STONE(0x800); can(permission) = (rank & permission) != 0
	constexpr std::array<int32_t, 7> bits{0x200, 0x8, 0x10, 0x4, 0x1000, 0x400, 0x800};
	for (size_t i = 0; i < MASKS.size(); i++) {
		EXPECT_TRUE(can(MASKS[i], bits[i])) << i;
		EXPECT_FALSE(can(MASKS[i], ~bits[i])) << i;
		EXPECT_FALSE(can(MASKS[i], 0)) << i;
	}
}

TEST(LegionCompanionsTest, RankIdsHistoryActionsAndEmblemTypes) {
	EXPECT_EQ(getRankId(LegionRank::BRIGADE_GENERAL), 0);
	EXPECT_EQ(getRankId(LegionRank::DEPUTY), 1);
	EXPECT_EQ(getRankId(LegionRank::CENTURION), 2);
	EXPECT_EQ(getRankId(LegionRank::LEGIONARY), 3);
	EXPECT_EQ(getRankId(LegionRank::VOLUNTEER), 4);
	// LegionHistoryAction.java:8-23 (7 to 10 unused)
	const std::vector<std::pair<int8_t, LegionHistoryAction_Type>> actions{{0, LegionHistoryAction_Type::LEGION}, {1, LegionHistoryAction_Type::LEGION},
		{2, LegionHistoryAction_Type::LEGION}, {3, LegionHistoryAction_Type::LEGION}, {4, LegionHistoryAction_Type::LEGION},
		{5, LegionHistoryAction_Type::LEGION}, {6, LegionHistoryAction_Type::LEGION}, {11, LegionHistoryAction_Type::REWARD},
		{12, LegionHistoryAction_Type::REWARD}, {13, LegionHistoryAction_Type::LEGION}, {14, LegionHistoryAction_Type::LEGION},
		{15, LegionHistoryAction_Type::WAREHOUSE}, {16, LegionHistoryAction_Type::WAREHOUSE}, {17, LegionHistoryAction_Type::WAREHOUSE},
		{18, LegionHistoryAction_Type::WAREHOUSE}};
	for (size_t i = 0; i < actions.size(); i++) {
		EXPECT_EQ(getId(static_cast<LegionHistoryAction>(i)), actions[i].first) << i;
		EXPECT_EQ(getType(static_cast<LegionHistoryAction>(i)), actions[i].second) << i;
	}
	EXPECT_EQ(getValue(LegionEmblemType::DEFAULT), 0);
	EXPECT_EQ(getValue(LegionEmblemType::CUSTOM), static_cast<int8_t>(0x80)) << "(byte) 0x80 is -128";
}

// ---- LegionMember ----

class LegionModelTest : public TeamTest {
protected:
	void TearDown() override {
		for (Member* m : stored)
			world::World::getInstance().removeObject(m->player());
		stored.clear();
		TeamTest::TearDown();
	}

	/** A member who is online: spawned, on a connection and stored in the World (World.getPlayer finds him) */
	Member& online(std::string_view name) {
		Member& m = addMember(name);
		world::World::getInstance().storeObject(m.player());
		stored.push_back(&m);
		return m;
	}

	std::vector<Member*> stored;
};

TEST_F(LegionModelTest, HasRightsOverEveryRankAndMaskWithTheDefaultPermissions) {
	runtime::Ref<Legion> legion = Legion::create(1, "Rights");
	runtime::Ref<LegionMember> member = LegionMember::create(100, *legion);
	// the defaults (Legion.java:30-33): deputy 0x1E0C, centurion 0x1C08, legionary 0x1800, volunteer 0x800
	const std::array<std::pair<LegionRank, std::array<bool, 7>>, 5> expected{{
		{LegionRank::BRIGADE_GENERAL, {true, true, true, true, true, true, true}},
		{LegionRank::DEPUTY, {true, true, false, true, true, true, true}},
		{LegionRank::CENTURION, {false, true, false, false, true, true, true}},
		{LegionRank::LEGIONARY, {false, false, false, false, true, false, true}},
		{LegionRank::VOLUNTEER, {false, false, false, false, false, false, true}},
	}};
	for (const auto& [rank, rights] : expected) {
		member->setRank(rank);
		for (size_t i = 0; i < MASKS.size(); i++)
			EXPECT_EQ(member->hasRights(MASKS[i]), rights[i]) << "rank " << static_cast<int32_t>(rank) << ", mask " << i;
	}
	// each rank reads its own permission field
	legion->setLegionPermissions(0, 0, 0, 0x10);
	member->setRank(LegionRank::VOLUNTEER);
	EXPECT_TRUE(member->hasRights(LegionPermissionsMask::KICK));
	member->setRank(LegionRank::DEPUTY);
	EXPECT_FALSE(member->hasRights(LegionPermissionsMask::KICK));
	EXPECT_TRUE(member->isBrigadeGeneral() == false);
	member->setRank(LegionRank::BRIGADE_GENERAL);
	EXPECT_TRUE(member->isBrigadeGeneral());
}

TEST_F(LegionModelTest, ChallengeScoreAndThePlayerData) {
	Member& a = addMember("Legionary");
	runtime::Ref<Legion> legion = Legion::create(2, "Data");
	runtime::Ref<LegionMember> member = LegionMember::create(a.player().getObjectId(), *legion);
	member->setChallengeScore(5);
	member->increaseChallengeScore(7);
	EXPECT_EQ(member->getChallengeScore(), 12);
	a.player().getCommonData()->setLastOnline(commons::database::Timestamp(std::chrono::milliseconds(1'700'000'123'456)));
	member->setPlayerData(a.player());
	EXPECT_EQ(member->getName(), "Legionary");
	EXPECT_EQ(member->getLevel(), a.player().getCommonData()->getLevel());
	EXPECT_EQ(member->getPlayerClass(), a.player().getCommonData()->getPlayerClass());
	EXPECT_EQ(member->getWorldId(), a.player().getCommonData()->getMapId());
	EXPECT_EQ(member->getLastOnlineEpochSeconds(), 1'700'000'123) << "getTime() / 1000";
	EXPECT_EQ(member->isOnline(), a.player().getCommonData()->isOnline());
}

// ---- Legion: members, levels, prices ----

TEST_F(LegionModelTest, MembersAreAddedUpToTheMaximumOfTheLevel) {
	ConfigScope<int32_t> l1(LegionConfig::LEGION_LEVEL1_MAX_MEMBERS, 2);
	ConfigScope<int32_t> l2(LegionConfig::LEGION_LEVEL2_MAX_MEMBERS, 3);
	runtime::Ref<Legion> legion = Legion::create(3, "Full");
	EXPECT_TRUE(legion->addLegionMember(11));
	EXPECT_TRUE(legion->addLegionMember(12));
	EXPECT_FALSE(legion->addLegionMember(13)) << "level 1 holds 2 (memberSize < LEVEL1_MAX_MEMBERS)";
	EXPECT_TRUE(legion->isMember(12));
	EXPECT_FALSE(legion->isMember(13));
	legion->setLegionLevel(2);
	EXPECT_TRUE(legion->addLegionMember(13));
	EXPECT_FALSE(legion->addLegionMember(14));
	legion->removeMember(12);
	EXPECT_FALSE(legion->isMember(12)) << "removed by value, not by index";
	EXPECT_TRUE(legion->isMember(11));
	EXPECT_EQ(legion->getMemberIds()->size(), 2);
	legion->setLegionLevel(9);
	EXPECT_FALSE(legion->addLegionMember(15)) << "no level 9: canAddMember answers false";
}

TEST_F(LegionModelTest, RequiredMembersKinahAndContributionPerLevel) {
	// distinct values per level, so a swapped case shows
	std::vector<std::unique_ptr<ConfigScope<int32_t>>> scopes;
	const std::array<std::atomic<int32_t>*, 7> requiredMembers{&LegionConfig::LEGION_LEVEL2_REQUIRED_MEMBERS, &LegionConfig::LEGION_LEVEL3_REQUIRED_MEMBERS,
		&LegionConfig::LEGION_LEVEL4_REQUIRED_MEMBERS, &LegionConfig::LEGION_LEVEL5_REQUIRED_MEMBERS, &LegionConfig::LEGION_LEVEL6_REQUIRED_MEMBERS,
		&LegionConfig::LEGION_LEVEL7_REQUIRED_MEMBERS, &LegionConfig::LEGION_LEVEL8_REQUIRED_MEMBERS};
	const std::array<std::atomic<int32_t>*, 7> kinah{&LegionConfig::LEGION_LEVEL2_REQUIRED_KINAH, &LegionConfig::LEGION_LEVEL3_REQUIRED_KINAH,
		&LegionConfig::LEGION_LEVEL4_REQUIRED_KINAH, &LegionConfig::LEGION_LEVEL5_REQUIRED_KINAH, &LegionConfig::LEGION_LEVEL6_REQUIRED_KINAH,
		&LegionConfig::LEGION_LEVEL7_REQUIRED_KINAH, &LegionConfig::LEGION_LEVEL8_REQUIRED_KINAH};
	const std::array<std::atomic<int32_t>*, 7> contribution{&LegionConfig::LEGION_LEVEL2_REQUIRED_CONTRIBUTION,
		&LegionConfig::LEGION_LEVEL3_REQUIRED_CONTRIBUTION, &LegionConfig::LEGION_LEVEL4_REQUIRED_CONTRIBUTION,
		&LegionConfig::LEGION_LEVEL5_REQUIRED_CONTRIBUTION, &LegionConfig::LEGION_LEVEL6_REQUIRED_CONTRIBUTION,
		&LegionConfig::LEGION_LEVEL7_REQUIRED_CONTRIBUTION, &LegionConfig::LEGION_LEVEL8_REQUIRED_CONTRIBUTION};
	for (int32_t i = 0; i < 7; i++) {
		scopes.push_back(std::make_unique<ConfigScope<int32_t>>(*requiredMembers[static_cast<size_t>(i)], i + 1));
		scopes.push_back(std::make_unique<ConfigScope<int32_t>>(*kinah[static_cast<size_t>(i)], 1000 * (i + 1)));
		scopes.push_back(std::make_unique<ConfigScope<int32_t>>(*contribution[static_cast<size_t>(i)], 77 * (i + 1)));
	}
	ConfigScope<int32_t> maxMembers(LegionConfig::LEGION_LEVEL1_MAX_MEMBERS, 100);
	runtime::Ref<Legion> legion = Legion::create(4, "Levels");
	for (int32_t level = 1; level <= 7; level++) {
		legion->setLegionLevel(level);
		legion->setMemberIds({});
		for (int32_t n = 0; n < level - 1; n++)
			legion->getMemberIds()->add(1000 + n);
		EXPECT_FALSE(legion->hasRequiredMembers()) << "level " << level << " with " << level - 1 << " members";
		legion->getMemberIds()->add(2000);
		EXPECT_TRUE(legion->hasRequiredMembers()) << "level " << level << " with " << level << " members (>=)";
		EXPECT_EQ(legion->getKinahPrice(), 1000 * level) << "level " << level;
		EXPECT_EQ(legion->getContributionPrice(), 77 * level) << "level " << level;
	}
	legion->setLegionLevel(8);
	EXPECT_FALSE(legion->hasRequiredMembers());
	EXPECT_EQ(legion->getKinahPrice(), 0);
	EXPECT_EQ(legion->getContributionPrice(), 0);
}

TEST_F(LegionModelTest, ContributionDisbandAnnouncementAndToString) {
	runtime::Ref<Legion> legion = Legion::create(5, "Misc");
	legion->addContributionPoints(40);
	legion->addContributionPoints(2);
	EXPECT_EQ(legion->getContributionPoints(), 42);
	EXPECT_FALSE(legion->isDisbanding());
	legion->setDisbandTime(1);
	EXPECT_TRUE(legion->isDisbanding());
	EXPECT_EQ(legion->getAnnouncement(), nullptr);
	runtime::Ref<Legion::Announcement> announcement = Legion::Announcement::create("hi", commons::database::Timestamp{});
	legion->setAnnouncement(announcement);
	EXPECT_EQ(legion->getAnnouncement().get(), announcement.get());
	runtime::Ref<LegionEmblem> emblem = LegionEmblem::create();
	legion->setLegionEmblem(emblem);
	EXPECT_EQ(legion->getLegionEmblem().get(), emblem.get());
	EXPECT_EQ(legion->toString(), "Legion [id=5, name=Misc]");
}

TEST_F(LegionModelTest, TheBrigadeGeneralAndTheMembersComeFromTheLegionService) {
	runtime::Ref<Legion> legion = Legion::create(6, "Nobody");
	legion->setMemberIds({31, 32});
	EXPECT_TRUE(legion->getMembers().empty()) << "no member is cached: getLegionMember answers null and the stream drops it";
	EXPECT_THROW(legion->getBrigadeGeneral(), runtime::NoSuchElementException) << "orElseThrow";
}

TEST_F(LegionModelTest, OnlinePlayersAreTheMembersTheWorldKnows) {
	Member& a = online("Alpha");
	Member& b = online("Bravo");
	runtime::Ref<Legion> legion = Legion::create(7, "Online");
	legion->setMemberIds({a.player().getObjectId(), 999999, b.player().getObjectId()});
	std::vector<runtime::Ptr<Player>> players = legion->getOnlinePlayers();
	ASSERT_EQ(players.size(), 2u);
	EXPECT_EQ(players[0].get(), &a.player());
	EXPECT_EQ(players[1].get(), &b.player());
}

TEST_F(LegionModelTest, TheBonusNeedsTenOnlineMembersAndIsSentOnce) {
	runtime::Ref<Legion> legion = Legion::create(8, "Bonus");
	std::vector<int32_t> ids;
	for (int32_t i = 0; i < 9; i++)
		ids.push_back(online("Member" + std::string(1, static_cast<char>('a' + i))).player().getObjectId());
	legion->setMemberIds(ids);
	legion->addBonus();
	EXPECT_FALSE(legion->hasBonus()) << "9 online";
	for (Member* m : stored)
		EXPECT_EQ(m->count(serverpackets::SM_ICON_INFO(1, true)), 0);
	ids.push_back(online("Memberj").player().getObjectId());
	legion->setMemberIds(ids);
	legion->addBonus();
	EXPECT_TRUE(legion->hasBonus()) << "10 online (>= 10)";
	for (Member* m : stored)
		EXPECT_EQ(m->count(serverpackets::SM_ICON_INFO(1, true)), 1) << m->player().getName();
	legion->addBonus();
	for (Member* m : stored)
		EXPECT_EQ(m->count(serverpackets::SM_ICON_INFO(1, true)), 1) << "compareAndSet: no second icon";
	legion->removeBonus();
	EXPECT_TRUE(legion->hasBonus()) << "still 10 online";
	world::World::getInstance().removeObject(stored.back()->player());
	stored.back()->clearSent();
	stored.pop_back();
	legion->removeBonus();
	EXPECT_FALSE(legion->hasBonus()) << "9 online (< 10)";
	for (Member* m : stored)
		EXPECT_EQ(m->count(serverpackets::SM_ICON_INFO(1, false)), 1) << m->player().getName();
}

// ---- Legion: history ----

TEST_F(LegionModelTest, HistoryIsPrependedPerTypeAndOnlyRewardAndWarehouseAreTrimmedAfterAYear) {
	runtime::Ref<Legion> legion = Legion::create(9, "History");
	const int32_t now = static_cast<int32_t>(commons::utils::currentTimeMillis() / 1000);
	const int32_t old = now - 366 * 24 * 3600;
	for (LegionHistoryAction_Type type : {LegionHistoryAction_Type::LEGION, LegionHistoryAction_Type::REWARD, LegionHistoryAction_Type::WAREHOUSE})
		EXPECT_TRUE(legion->getHistory(type).empty());
	// LEGION: an old entry stays
	runtime::Ref<LegionHistoryEntry> oldJoin = LegionHistoryEntry::create(1, old, LegionHistoryAction::JOIN, "a", "");
	runtime::Ref<LegionHistoryEntry> newKick = LegionHistoryEntry::create(2, now, LegionHistoryAction::KICK, "b", "");
	legion->setHistory({{LegionHistoryAction_Type::LEGION, {oldJoin}}});
	EXPECT_TRUE(legion->addHistory(*newKick).empty());
	std::vector<runtime::Ptr<LegionHistoryEntry>> legionHistory = legion->getHistory(LegionHistoryAction_Type::LEGION);
	ASSERT_EQ(legionHistory.size(), 2u);
	EXPECT_EQ(legionHistory[0].get(), newKick.get()) << "addFirst";
	EXPECT_EQ(legionHistory[1].get(), oldJoin.get()) << "LEGION history is never trimmed";
	// WAREHOUSE: the entries older than 365 days at the end are removed and returned, newest removed last
	runtime::Ref<LegionHistoryEntry> older = LegionHistoryEntry::create(3, old - 10, LegionHistoryAction::ITEM_DEPOSIT, "c", "");
	runtime::Ref<LegionHistoryEntry> oldest = LegionHistoryEntry::create(4, old - 20, LegionHistoryAction::KINAH_DEPOSIT, "d", "");
	runtime::Ref<LegionHistoryEntry> recent = LegionHistoryEntry::create(5, now - 100, LegionHistoryAction::ITEM_WITHDRAW, "e", "");
	legion->setHistory({{LegionHistoryAction_Type::WAREHOUSE, {recent, older, oldest}}});
	EXPECT_TRUE(legion->getHistory(LegionHistoryAction_Type::LEGION).empty()) << "setHistory replaces every type";
	runtime::Ref<LegionHistoryEntry> deposit = LegionHistoryEntry::create(6, now, LegionHistoryAction::KINAH_WITHDRAW, "f", "");
	std::vector<runtime::Ref<LegionHistoryEntry>> removed = legion->addHistory(*deposit);
	ASSERT_EQ(removed.size(), 2u);
	EXPECT_EQ(removed[0].get(), oldest.get());
	EXPECT_EQ(removed[1].get(), older.get());
	std::vector<runtime::Ptr<LegionHistoryEntry>> warehouse = legion->getHistory(LegionHistoryAction_Type::WAREHOUSE);
	ASSERT_EQ(warehouse.size(), 2u);
	EXPECT_EQ(warehouse[0].get(), deposit.get());
	EXPECT_EQ(warehouse[1].get(), recent.get());
	// REWARD: trimmed too
	runtime::Ref<LegionHistoryEntry> oldReward = LegionHistoryEntry::create(7, old, LegionHistoryAction::DEFENSE, "100", "1011");
	legion->setHistory({{LegionHistoryAction_Type::REWARD, {oldReward}}});
	runtime::Ref<LegionHistoryEntry> reward = LegionHistoryEntry::create(8, now, LegionHistoryAction::OCCUPATION, "200", "1011");
	removed = legion->addHistory(*reward);
	ASSERT_EQ(removed.size(), 1u);
	EXPECT_EQ(removed[0].get(), oldReward.get());
	EXPECT_EQ(legion->getHistory(LegionHistoryAction_Type::REWARD).size(), 1u);
}

// ---- LegionWarehouse ----

TEST_F(LegionModelTest, WarehouseLimitFollowsTheLevel) {
	runtime::Ref<Legion> legion = Legion::create(10, "Storage");
	EXPECT_EQ(legion->getWarehouseExpansions(), 0);
	EXPECT_EQ(legion->getLegionWarehouse().getLimit(), 24) << "(3 + 0) rows of 8";
	for (int32_t level = 2; level <= 8; level++) {
		legion->setLegionLevel(level);
		EXPECT_EQ(legion->getLegionWarehouse().getLimit(), (3 + level - 1) * 8) << "level " << level;
	}
	EXPECT_THROW(legion->getLegionWarehouse().updateLimit(-1), runtime::IllegalArgumentException);
	EXPECT_THROW(legion->getLegionWarehouse().setLimit(10), runtime::UnsupportedOperationException);
	EXPECT_THROW(legion->getLegionWarehouse().setOwner(nullptr), runtime::UnsupportedOperationException);
	EXPECT_THROW(legion->getLegionWarehouse().tryDecreaseKinah(1), runtime::UnsupportedOperationException) << "behind proxy";
	EXPECT_THROW(legion->getLegionWarehouse().decreaseKinah(1), runtime::UnsupportedOperationException) << "behind proxy";
}

TEST_F(LegionModelTest, TheWarehouseUserIsACompareAndSet) {
	runtime::Ref<Legion> legion = Legion::create(11, "InUse");
	model::team::legion::LegionWarehouse& warehouse = legion->getLegionWarehouse();
	EXPECT_EQ(warehouse.getCurrentUser(), 0);
	EXPECT_FALSE(warehouse.unsetInUse(5)) << "not in use";
	// two threads race for the warehouse: exactly one wins
	for (int32_t round = 0; round < 50; round++) {
		std::atomic<int32_t> winners{0};
		std::thread first([&] { winners += warehouse.setInUse(101) ? 1 : 0; });
		std::thread second([&] { winners += warehouse.setInUse(102) ? 1 : 0; });
		first.join();
		second.join();
		ASSERT_EQ(winners.load(), 1) << "round " << round;
		const int32_t user = warehouse.getCurrentUser();
		EXPECT_TRUE(user == 101 || user == 102);
		EXPECT_FALSE(warehouse.unsetInUse(user == 101 ? 102 : 101)) << "only the user releases it";
		EXPECT_TRUE(warehouse.unsetInUse(user));
		EXPECT_EQ(warehouse.getCurrentUser(), 0);
	}
}

// ---- LegionEmblem ----

TEST_F(LegionModelTest, EmblemPersistentStateKeepsNewAgainstUpdateRequired) {
	runtime::Ref<LegionEmblem> emblem = LegionEmblem::create();
	EXPECT_EQ(emblem->getPersistentState(), Persistable::PersistentState::NEW);
	emblem->setEmblem(3, 255, 10, 20, 30, LegionEmblemType::DEFAULT, nullptr);
	EXPECT_EQ(emblem->getPersistentState(), Persistable::PersistentState::NEW) << "UPDATE_REQUIRED does not replace NEW";
	EXPECT_EQ(emblem->getEmblemId(), 3);
	EXPECT_EQ(emblem->getColor_a(), static_cast<int8_t>(255)) << "(byte) 255";
	emblem->setPersistentState(Persistable::PersistentState::UPDATED);
	emblem->setEmblem(4, 0, 0, 0, 0, LegionEmblemType::CUSTOM, nullptr);
	EXPECT_EQ(emblem->getPersistentState(), Persistable::PersistentState::UPDATE_REQUIRED);
	EXPECT_EQ(emblem->getEmblemType(), LegionEmblemType::DEFAULT) << "CUSTOM without data falls back to DEFAULT";
	emblem->setCustomEmblemData(runtime::Array<int8_t>::of({1, 2}));
	EXPECT_EQ(emblem->getEmblemType(), LegionEmblemType::CUSTOM);
	EXPECT_EQ(emblem->getCustomEmblemData()->length(), 2);
}

TEST_F(LegionModelTest, EmblemTheUploadAccumulatesChunksSizedByTheUploadedSize) {
	runtime::Ref<LegionEmblem> emblem = LegionEmblem::create();
	emblem->setUploading(true);
	emblem->setUploadSize(5);
	emblem->addUploadedSize(3);
	emblem->addUploadData(runtime::Array<int8_t>::of({1, 2, 3}));
	emblem->addUploadedSize(2);
	emblem->addUploadData(runtime::Array<int8_t>::of({4, 5}));
	ASSERT_EQ(emblem->getUploadData()->length(), 5);
	for (int32_t i = 0; i < 5; i++)
		EXPECT_EQ(emblem->getUploadData()->get(i), i + 1);
	EXPECT_THROW(emblem->addUploadData(runtime::Array<int8_t>::of({6})), runtime::ArrayIndexOutOfBoundsException)
		<< "Java: new byte[uploadedSize] has no room for a chunk whose size was not added";
	emblem->resetUploadSettings();
	EXPECT_FALSE(emblem->isUploading());
	EXPECT_EQ(emblem->getUploadedSize(), 0);
	EXPECT_EQ(emblem->getUploadData(), nullptr);
	EXPECT_EQ(emblem->getUploadSize(), 5) << "resetUploadSettings leaves the upload size";
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing::team
