// The M5g alliance decoders (m5g-plan.md §16.3 item 7) against bodies written out field by field in the Java writeImpl order (m5a-plan.md D9):
// every case also relies on the decoders' exact-consumption check. No case includes or consults a C++ serverpackets header.

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "decoders/AllianceDecoders.h"

#include "NetworkTestSupport.h"

namespace aion::gameserver::scenario::decoders {
namespace {

using network::test::PacketWriter;

/** SM_ALLIANCE_INFO.writeImpl: four groups, leader 2002, one vice captain, default loot rules, TeamType.ALLIANCE, no league */
TEST(AllianceDecodersTest, AllianceInfoWithoutALeague) {
	PacketWriter w;
	w.H(4).D(5005).D(2002).D(210010000).D(3003).D(0).D(0).D(0);
	w.D(1).D(0).D(0).D(2).D(2).D(2).D(2).D(2).D(0x02).C(0).D(0x3F).D(0).D(0);
	for (int32_t a = 0; a < 4; a++)
		w.D(a).D(1000 + a);
	w.D(1300984).S("Bravo");
	const AllianceInfo info = decodeAllianceInfo(w.data);
	EXPECT_EQ(info.groupSize, 4);
	EXPECT_EQ(info.allianceId, 5005);
	EXPECT_EQ(info.leaderId, 2002);
	EXPECT_EQ(info.viceCaptains, (std::vector<int32_t>{3003}));
	EXPECT_EQ(info.lootWords, (std::vector<int32_t>{1, 0, 0, 2, 2, 2, 2, 2}));
	EXPECT_EQ(info.type, 0x3F);
	EXPECT_EQ(info.messageId, 1300984);
	EXPECT_EQ(info.message, "Bravo");
	EXPECT_EQ(info.leagueAlliances, 0);
	PacketWriter bad;
	bad.H(4).D(5005).D(2002).D(210010000).D(0).D(0).D(0).D(0);
	bad.D(1).D(0).D(0).D(2).D(2).D(2).D(2).D(2).D(0x02).C(0).D(0x3F).D(0).D(0);
	for (int32_t a = 0; a < 4; a++)
		bad.D(a).D(1001 + a);
	bad.D(0).S("");
	EXPECT_THROW(decodeAllianceInfo(bad.data), DecodeError) << "the literal group ids 1000-1003";
}

/** the common head of SM_ALLIANCE_MEMBER_INFO.writeImpl */
void memberHead(PacketWriter& w, int32_t objectId, uint8_t event) {
	w.D(1001).D(objectId).D(100).D(90).D(50).D(40).D(60).D(60).D(0).D(210010000).D(210010000).F(1.5f).F(2.5f).F(3.5f).C(0).C(1).C(3);
	w.C(event).C(1).C(0).C(0);
}

TEST(AllianceDecodersTest, MemberInfoJoinWithEffectsAndTheGroupChange) {
	PacketWriter join;
	memberHead(join, 7, ALLIANCE_EVENT_JOIN);
	join.S("Alpha").D(0).D(0).C(0xFF).H(1).D(7).H(3195).C(1).C(0).D(30000);
	for (int i = 0; i < 8; i++)
		join.D(0);
	const AllianceMemberInfo joined = decodeAllianceMemberInfo(join.data);
	EXPECT_EQ(joined.allianceGroupId, 1001);
	EXPECT_EQ(joined.name, std::optional<std::string>("Alpha"));
	EXPECT_FALSE(joined.groupChange);
	ASSERT_EQ(joined.effects.size(), 1u);
	EXPECT_EQ(joined.effects[0].skillId, 3195);
	PacketWriter change;
	memberHead(change, 7, ALLIANCE_EVENT_JOIN); // MEMBER_GROUP_CHANGE's id is 5 too: name only
	change.S("Alpha");
	const AllianceMemberInfo changed = decodeAllianceMemberInfo(change.data);
	EXPECT_TRUE(changed.groupChange);
	EXPECT_TRUE(changed.effects.empty());
}

TEST(AllianceDecodersTest, MemberInfoOfflineEnterMovementAndUpdateEffects) {
	PacketWriter offline;
	memberHead(offline, 7, ALLIANCE_EVENT_ENTER_OFFLINE);
	offline.S("Alpha").D(0).D(0).H(0);
	EXPECT_EQ(decodeAllianceMemberInfo(offline.data).event, ALLIANCE_EVENT_ENTER_OFFLINE);
	PacketWriter move;
	memberHead(move, 7, ALLIANCE_EVENT_MOVEMENT);
	EXPECT_FALSE(decodeAllianceMemberInfo(move.data).name);
	PacketWriter effects;
	memberHead(effects, 7, ALLIANCE_EVENT_UPDATE_EFFECTS);
	effects.D(0).D(0).C(1).H(0);
	for (int i = 0; i < 8; i++)
		effects.D(0);
	EXPECT_EQ(decodeAllianceMemberInfo(effects.data).slot, std::optional<uint8_t>(1));
	PacketWriter unknown;
	memberHead(unknown, 7, 9);
	EXPECT_THROW(decodeAllianceMemberInfo(unknown.data), DecodeError);
}

TEST(AllianceDecodersTest, ReadyCheck) {
	PacketWriter w;
	w.D(7).C(5);
	const AllianceReadyCheck check = decodeAllianceReadyCheck(w.data);
	EXPECT_EQ(check.playerObjectId, 7);
	EXPECT_EQ(check.statusCode, 5);
}

} // namespace
} // namespace aion::gameserver::scenario::decoders
