// P5-10c alliance model (m5g-plan.md AL-01, §16.3 item 2): PlayerAlliance's four alliance groups 1000-1003, getOpenAllianceGroup filling them
// six by six and failing at the 25th member, the member's alliance group and alliance id, the removal through the member's group, the
// captain rules, the exp levels, the loot rules an alliance group reads from its alliance, the lookup of a missing group, and the per-kind
// team lock classes of header request m5g-1 (§16.2). No service: the members are added the way PlayerAllianceEnteredEvent does
// (alliance.addMember), and each test removes them again. The party fixture is tests/team/P5-10b's.
//
// Expectations are derived by hand from PlayerAlliance.java:26-143 and PlayerAllianceGroup.java:13-54.

#include "../P5-10b/TeamTestSupport.h"

#include <cstdint>
#include <optional>
#include <vector>

#include "aion/gameserver/model/team/alliance/PlayerAlliance.h"
#include "aion/gameserver/model/team/alliance/PlayerAllianceGroup.h"
#include "aion/gameserver/model/team/alliance/PlayerAllianceMember.h"
#include "aion/gameserver/model/team/common/legacy/LootGroupRules.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/sync/LockOrderValidator.h"
#include "aion/commons/utils/Exception.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing::team {
namespace {

using model::team::TeamType;
using model::team::alliance::PlayerAlliance;
using model::team::alliance::PlayerAllianceGroup;
using model::team::alliance::PlayerAllianceMember;

class PlayerAllianceModelTest : public TeamTest {
protected:
	void TearDown() override {
		if (alliance) {
			for (const runtime::Ptr<model::gameobjects::AionObject>& object : alliance->getMembers())
				alliance->removeMember(object->getObjectId());
		}
		alliance = nullptr;
		TeamTest::TearDown();
	}

	/** An alliance led by the first member, every member added in order (PlayerAllianceEnteredEvent's alliance.addMember) */
	PlayerAlliance& allianceOf(const std::vector<Member*>& list) {
		runtime::Ref<PlayerAllianceMember> leader = PlayerAllianceMember::create(list.front()->player());
		alliance = PlayerAlliance::create(*leader, TeamType::ALLIANCE);
		alliance->addMember(*leader);
		for (size_t i = 1; i < list.size(); i++)
			alliance->addMember(*PlayerAllianceMember::create(list[i]->player()));
		return *alliance;
	}

	std::vector<Member*> players(int32_t n) {
		std::vector<Member*> list;
		for (int32_t i = 0; i < n; i++)
			list.push_back(&addMember("Ally" + std::to_string(i + 1), 100.0f + static_cast<float>(i)));
		return list;
	}

	runtime::Ref<PlayerAlliance> alliance;
};

/** PlayerAlliance.java:26-33, 125-131: four empty groups 1000-1003, the leader set but no member yet */
TEST_F(PlayerAllianceModelTest, TheConstructorCreatesFourAllianceGroups) {
	Member& a = addMember("Alpha");
	runtime::Ref<PlayerAllianceMember> leader = PlayerAllianceMember::create(a.player());
	alliance = PlayerAlliance::create(*leader, TeamType::ALLIANCE);
	EXPECT_EQ(alliance->groupSize(), 4);
	for (int32_t id = 1000; id <= 1003; id++) {
		runtime::Ptr<PlayerAllianceGroup> group = alliance->getAllianceGroup(id);
		EXPECT_EQ(group->getTeamId(), id);
		EXPECT_EQ(group->size(), 0);
		EXPECT_EQ(group->getAlliance().get(), alliance.get());
	}
	EXPECT_EQ(alliance->getLeader().get(), leader.get());
	EXPECT_EQ(alliance->size(), 0) << "setLeader does not add the member";
	EXPECT_NE(alliance->getTeamId(), 0) << "IDFactory.nextId()";
	EXPECT_EQ(alliance->getTeamType(), TeamType::ALLIANCE);
	EXPECT_EQ(alliance->getMaxMemberCount(), 24);
	EXPECT_EQ(alliance->getAllianceGroup(1000)->getMaxMemberCount(), 6);
	EXPECT_FALSE(alliance->isInLeague());
}

/** getAllianceGroup: Objects.requireNonNull(allianceGroup, "No such alliance group " + id) (PlayerAlliance.java:84-88) */
TEST_F(PlayerAllianceModelTest, AMissingAllianceGroupIsANullPointerException) {
	Member& a = addMember("Alpha");
	allianceOf({&a});
	EXPECT_THROW(alliance->getAllianceGroup(999), runtime::NullPointerException);
	EXPECT_THROW(alliance->getAllianceGroup(std::nullopt), runtime::NullPointerException);
}

/** addMember: super.addMember, then the first open group 1000..1003 (PlayerAlliance.java:35-40, 70-82; PlayerAllianceGroup.java:18-23) */
TEST_F(PlayerAllianceModelTest, MembersFillTheGroupsSixBySix) {
	std::vector<Member*> list = players(8);
	allianceOf(list);
	EXPECT_EQ(alliance->size(), 8);
	EXPECT_EQ(alliance->getAllianceGroup(1000)->size(), 6);
	EXPECT_EQ(alliance->getAllianceGroup(1001)->size(), 2);
	EXPECT_EQ(alliance->getAllianceGroup(1002)->size(), 0);
	for (size_t i = 0; i < list.size(); i++) {
		const int32_t expected = i < 6 ? 1000 : 1001;
		Player& player = list[i]->player();
		ASSERT_TRUE(player.getPlayerAllianceGroup()) << i;
		EXPECT_EQ(player.getPlayerAllianceGroup()->getTeamId(), expected) << i;
		EXPECT_EQ(player.getPlayerAlliance().get(), alliance.get()) << i;
		EXPECT_EQ(alliance->getMember(player.getObjectId())->getAllianceId(), expected) << i;
		EXPECT_EQ(alliance->getMember(player.getObjectId()).get(), alliance->getAllianceGroup(expected)->getMember(player.getObjectId()).get())
			<< "the same member object in both teams";
	}
}

/** getOpenAllianceGroup: IllegalStateException("All alliance groups are full.") (PlayerAlliance.java:70-82); the member is already in the
 * alliance's own map when it throws (super.addMember runs first, :37) */
TEST_F(PlayerAllianceModelTest, The25thMemberFindsNoOpenGroup) {
	std::vector<Member*> list = players(25);
	std::vector<Member*> first(list.begin(), list.begin() + 24);
	allianceOf(first);
	for (int32_t id = 1000; id <= 1003; id++)
		EXPECT_TRUE(alliance->getAllianceGroup(id)->isFull()) << id;
	EXPECT_TRUE(alliance->isFull());
	runtime::Ref<PlayerAllianceMember> extra = PlayerAllianceMember::create(list[24]->player());
	EXPECT_THROW(alliance->addMember(*extra), commons::utils::IllegalStateException);
	EXPECT_TRUE(alliance->hasMember(list[24]->player().getObjectId())) << "Java: super.addMember ran before the throw";
	EXPECT_FALSE(list[24]->player().getPlayerAllianceGroup());
	// TearDown's removeMember reaches onRemoveMember with a member that has no group: Java's NullPointerException
	EXPECT_THROW(alliance->removeMember(list[24]->player().getObjectId()), runtime::NullPointerException);
}

/** onRemoveMember: member.getPlayerAllianceGroup().removeMember(member) (PlayerAlliance.java:42-45; PlayerAllianceGroup.java:25-28) */
TEST_F(PlayerAllianceModelTest, ARemovedMemberLeavesItsAllianceGroup) {
	std::vector<Member*> list = players(3);
	allianceOf(list);
	Player& b = list[1]->player();
	runtime::Ptr<PlayerAllianceMember> removed = alliance->removeMember(b.getObjectId());
	ASSERT_TRUE(removed);
	EXPECT_EQ(&removed->getPlayer(), &b);
	EXPECT_FALSE(alliance->hasMember(b.getObjectId()));
	EXPECT_FALSE(alliance->getAllianceGroup(1000)->hasMember(b.getObjectId()));
	EXPECT_EQ(alliance->getAllianceGroup(1000)->size(), 2);
	EXPECT_FALSE(b.getPlayerAllianceGroup());
	EXPECT_FALSE(b.getPlayerAlliance());
	// the open group is the first with room again: a new member goes to 1000
	Member& d = addMember("Delta", 120.0f);
	alliance->addMember(*PlayerAllianceMember::create(d.player()));
	EXPECT_EQ(d.player().getPlayerAllianceGroup()->getTeamId(), 1000);
}

/** isViceCaptain: viceCaptainIds.contains(objectId); isSomeCaptain: the leader or a vice captain (PlayerAlliance.java:94-100) */
TEST_F(PlayerAllianceModelTest, CaptainsAreTheLeaderAndTheViceCaptains) {
	std::vector<Member*> list = players(3);
	allianceOf(list);
	Player& a = list[0]->player();
	Player& b = list[1]->player();
	Player& c = list[2]->player();
	EXPECT_TRUE(alliance->isSomeCaptain(a));
	EXPECT_FALSE(alliance->isViceCaptain(a));
	EXPECT_FALSE(alliance->isSomeCaptain(b));
	alliance->getViceCaptainIds().add(b.getObjectId());
	EXPECT_TRUE(alliance->isViceCaptain(b));
	EXPECT_TRUE(alliance->isSomeCaptain(b));
	EXPECT_FALSE(alliance->isSomeCaptain(c));
}

/** getMinExpPlayerLevel from 99 down, getMaxExpPlayerLevel from 1 up over the members (PlayerAlliance.java:52-68); an alliance group answers
 * 0 for both (PlayerAllianceGroup.java:35-45) */
TEST_F(PlayerAllianceModelTest, TheExpLevelsSpanTheMembers) {
	std::vector<Member*> list = players(3);
	allianceOf(list);
	list[0]->player().getCommonData()->setLevel(2);
	list[1]->player().getCommonData()->setLevel(6);
	list[2]->player().getCommonData()->setLevel(4);
	EXPECT_EQ(alliance->getMinExpPlayerLevel(), 2);
	EXPECT_EQ(alliance->getMaxExpPlayerLevel(), 6);
	EXPECT_EQ(alliance->getAllianceGroup(1000)->getMinExpPlayerLevel(), 0);
	EXPECT_EQ(alliance->getAllianceGroup(1000)->getMaxExpPlayerLevel(), 0);
	for (const runtime::Ptr<model::gameobjects::AionObject>& object : alliance->getMembers())
		alliance->removeMember(object->getObjectId());
	EXPECT_EQ(alliance->getMinExpPlayerLevel(), 99) << "no member";
	EXPECT_EQ(alliance->getMaxExpPlayerLevel(), 1);
}

/** getLootGroupRules: the alliance's own without a league (PlayerAlliance.java:139-142); an alliance group reads its alliance's
 * (PlayerAllianceGroup.java:51-54), also after the alliance's rules change */
TEST_F(PlayerAllianceModelTest, AnAllianceGroupLootsByItsAlliancesRules) {
	std::vector<Member*> list = players(2);
	allianceOf(list);
	runtime::Ptr<PlayerAllianceGroup> group = alliance->getAllianceGroup(1001);
	EXPECT_EQ(group->getLootGroupRules().get(), alliance->getLootGroupRules().get());
	alliance->setLootGroupRules(model::team::common::legacy::LootGroupRules::create());
	EXPECT_EQ(group->getLootGroupRules().get(), alliance->getLootGroupRules().get());
}

/** m5g-1 (§16.2): each team kind's teamLock reports a lock class of its own, so an alliance group locked under its alliance is no same-class
 * nesting */
TEST_F(PlayerAllianceModelTest, EachTeamKindHasItsOwnLockClass) {
	std::vector<Member*> list = players(2);
	PlayerAlliance& value = allianceOf(list);
	runtime::LockOrderValidator& validator = runtime::LockOrderValidator::getInstance();
	if (!validator.isEnabled())
		GTEST_SKIP() << "the lock-order validator runs in checked builds only";
	validator.clearReports();
	bool ran = false;
	value.forEach([&](model::gameobjects::AionObject&) {
		// the alliance's lock is held: the group's forEach takes PlayerAllianceGroup::teamLock under PlayerAlliance::teamLock
		value.getAllianceGroup(1000)->forEach([&](model::gameobjects::AionObject&) { ran = true; });
	});
	EXPECT_TRUE(ran);
	for (const runtime::LockOrderValidator::Report& report : validator.getReports())
		ADD_FAILURE() << report.text;
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing::team
