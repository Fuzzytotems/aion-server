// The M5g party decoders (m5g-plan.md H-02) against bodies written out field by field in the Java writeImpl order (m5a-plan.md D9): every case
// also relies on the decoders' exact-consumption check, so a field read with the wrong width or in the wrong order fails. No case includes or
// consults a C++ serverpackets header.

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "decoders/TeamDecoders.h"

#include "NetworkTestSupport.h"

namespace aion::gameserver::scenario::decoders {
namespace {

using network::test::PacketWriter;

/** SM_GROUP_INFO.java:31-47 for a group with the default LootGroupRules (LootGroupRules.java:32-40) and TeamType.GROUP (0x3F, 0) */
TEST(TeamDecodersTest, GroupInfo) {
	PacketWriter w;
	w.D(1001).D(2002).D(210010000).D(1).D(0).D(0).D(2).D(2).D(2).D(2).D(2).D(0x02).C(0).D(0x3F).D(0).D(0).S("");
	const GroupInfo info = decodeGroupInfo(w.data);
	EXPECT_EQ(info.groupId, 1001);
	EXPECT_EQ(info.leaderId, 2002);
	EXPECT_EQ(info.mapId, 210010000);
	EXPECT_EQ(info.lootWords, (std::vector<int32_t>{1, 0, 0, 2, 2, 2, 2, 2}));
	EXPECT_EQ(info.type, 0x3F);
	EXPECT_EQ(info.subType, 0);
	PacketWriter bad;
	bad.D(1).D(2).D(3).D(1).D(0).D(0).D(2).D(2).D(2).D(2).D(2).D(0x03).C(0).D(0x3F).D(0).D(0).S("");
	EXPECT_THROW(decodeGroupInfo(bad.data), DecodeError) << "the literal writeD(0x02)";
}

/** the common head of SM_GROUP_MEMBER_INFO.java:72-104 */
void memberHead(PacketWriter& w, int32_t objectId, uint8_t event) {
	w.D(1001).D(objectId).D(100).D(90).D(50).D(40).D(60).D(60).D(0).D(210010000).D(210010000).F(1.5f).F(2.5f).F(3.5f).C(0).C(1).C(3);
	w.C(event).C(1).C(0).C(0);
}

TEST(TeamDecodersTest, GroupMemberInfoJoinAndMovement) {
	PacketWriter join;
	memberHead(join, 7, GROUP_EVENT_JOIN);
	join.S("Alpha");
	const GroupMemberInfo joined = decodeGroupMemberInfo(join.data);
	EXPECT_EQ(joined.objectId, 7);
	EXPECT_EQ(joined.maxHp, 100);
	EXPECT_EQ(joined.currentMp, 40);
	EXPECT_EQ(joined.level, 3);
	EXPECT_EQ(joined.genderId, 1);
	EXPECT_EQ(joined.x, 1.5f);
	EXPECT_EQ(joined.name, std::optional<std::string>("Alpha"));
	PacketWriter move;
	memberHead(move, 7, GROUP_EVENT_MOVEMENT);
	const GroupMemberInfo moved = decodeGroupMemberInfo(move.data);
	EXPECT_EQ(moved.event, GROUP_EVENT_MOVEMENT);
	EXPECT_FALSE(moved.name);
}

/** UPDATE_EFFECTS (:115-132): two unk D, the slot, the effect count, the entries, then one D per SkillTargetSlot (eight) */
TEST(TeamDecodersTest, GroupMemberInfoUpdateEffects) {
	PacketWriter w;
	memberHead(w, 7, GROUP_EVENT_UPDATE_EFFECTS);
	w.D(0).D(0).C(1).H(1).D(7).H(3195).C(1).C(0).D(5000);
	for (int i = 0; i < 8; i++)
		w.D(0);
	const GroupMemberInfo info = decodeGroupMemberInfo(w.data);
	EXPECT_EQ(info.slot, std::optional<uint8_t>(1));
	ASSERT_EQ(info.effects.size(), 1u);
	EXPECT_EQ(info.effects[0].skillId, 3195);
	EXPECT_EQ(info.effects[0].remainingMillis, 5000);
	PacketWriter shortTail;
	memberHead(shortTail, 7, GROUP_EVENT_UPDATE_EFFECTS);
	shortTail.D(0).D(0).C(1).H(0);
	for (int i = 0; i < 7; i++)
		shortTail.D(0);
	EXPECT_THROW(decodeGroupMemberInfo(shortTail.data), DecodeError) << "seven slot words instead of eight";
}

TEST(TeamDecodersTest, GroupMemberInfoEnterIsFullSlots) {
	PacketWriter w;
	memberHead(w, 7, GROUP_EVENT_ENTER);
	w.S("Alpha").D(0).D(0).C(SKILL_TARGET_SLOT_FULLSLOTS).H(0);
	for (int i = 0; i < 8; i++)
		w.D(0);
	const GroupMemberInfo info = decodeGroupMemberInfo(w.data);
	EXPECT_EQ(info.name, std::optional<std::string>("Alpha"));
	EXPECT_EQ(info.slot, std::optional<uint8_t>(SKILL_TARGET_SLOT_FULLSLOTS));
}

/** SM_LEAVE_GROUP_MEMBER.java:14-18 */
TEST(TeamDecodersTest, LeaveGroupMember) {
	PacketWriter w;
	w.D(0).C(0).D(0x3F).D(0).H(0);
	EXPECT_NO_THROW(decodeLeaveGroupMember(w.data));
	PacketWriter bad;
	bad.D(0).C(0).D(0x02).D(0).H(0);
	EXPECT_THROW(decodeLeaveGroupMember(bad.data), DecodeError);
}

/** SM_SHOW_BRAND.java:31-36 */
TEST(TeamDecodersTest, ShowBrand) {
	PacketWriter w;
	w.H(2).D(1).D(1).D(4242).D(1).D(2).D(0);
	EXPECT_EQ(decodeShowBrand(w.data).brands, (std::vector<std::pair<int32_t, int32_t>>{{1, 4242}, {2, 0}}));
}

/** SM_GROUP_LOOT.java:42-52 */
TEST(TeamDecodersTest, GroupLoot) {
	PacketWriter w;
	w.D(1001).D(3).D(1).D(162000002).C(0).C(0).C(0).D(55).C(2).D(7).D(-1);
	const GroupLoot loot = decodeGroupLoot(w.data);
	EXPECT_EQ(loot.index, 3);
	EXPECT_EQ(loot.distributionId, 2);
	EXPECT_EQ(loot.playerId, 7);
	EXPECT_EQ(loot.luck, -1) << "(int) luck of 0xFFFFFFFF";
}

/** SM_GROUP_DATA_EXCHANGE.java:30-36: action 1 has no unk2 */
TEST(TeamDecodersTest, GroupDataExchange) {
	const std::vector<uint8_t> data{9, 8, 7};
	PacketWriter w;
	w.C(0).C(0).D(3).B(data);
	const GroupDataExchange zero = decodeGroupDataExchange(w.data);
	EXPECT_EQ(zero.unk2, std::optional<uint8_t>(0));
	EXPECT_EQ(zero.data, data);
	PacketWriter one;
	one.C(1).D(3).B(data);
	EXPECT_FALSE(decodeGroupDataExchange(one.data).unk2);
}

/** SM_ABYSS_RANK_UPDATE.java:26-40, action 1: the team id */
TEST(TeamDecodersTest, AbyssRankUpdate) {
	PacketWriter w;
	w.C(1).D(7).D(1001);
	const AbyssRankUpdate update = decodeAbyssRankUpdate(w.data);
	EXPECT_EQ(update.objectId, 7);
	EXPECT_EQ(update.value, 1001);
}

/** SM_FIND_GROUP showRecruitments and removeRecruitment */
TEST(TeamDecodersTest, FindGroup) {
	PacketWriter list;
	list.C(0).H(1).H(1).D(123).D(7).C(1).C(0).C(0).C(16).C(0).S("m5g lfg").S("Delta").C(1).C(3).C(3).D(123);
	const FindGroup found = decodeFindGroup(list.data);
	ASSERT_EQ(found.recruitments.size(), 1u);
	EXPECT_EQ(found.recruitments[0].objectId, 7);
	EXPECT_EQ(found.recruitments[0].soloFlag, 16);
	EXPECT_EQ(found.recruitments[0].message, "m5g lfg");
	PacketWriter removal;
	removal.C(1).D(7).C(1).C(0).C(0).C(16);
	const FindGroup removed = decodeFindGroup(removal.data);
	EXPECT_EQ(removed.removedId, 7);
	EXPECT_EQ(removed.removedSoloFlag, 16);
}

/** SM_RECALLED_BY_OTHER.java: open and close */
TEST(TeamDecodersTest, RecalledByOther) {
	PacketWriter open;
	open.C(0).S("Alpha").H(3777).H(30);
	const RecalledByOther question = decodeRecalledByOther(open.data);
	EXPECT_TRUE(question.open);
	EXPECT_EQ(question.skillId, 3777);
	EXPECT_EQ(question.seconds, 30);
	PacketWriter close;
	close.C(1).S("").H(0).H(0);
	EXPECT_FALSE(decodeRecalledByOther(close.data).open);
}

} // namespace
} // namespace aion::gameserver::scenario::decoders
