// The G-02 quest decoders (m5d-plan.md §3.9, G-02) against byte vectors written from the Java writeImpl, in the style of
// EconomyDecodersTest.cpp: a hand-built body with the offset of every field where the layout is flat, a builder written out field by field in
// Java order where it is a list, and every case asserts the body size Java produces. The values are the gate's own (m5d-plan.md §10.3), taken
// from the data through the oracle:
// - quest 1101's three states (`oracle.py m5d-quest --quest 1101`: ADD START (3) vars 0, UPDATE REWARD (4) vars [1], UPDATE COMPLETE (5)
//   vars 0) and 1102's kill counts (`--quest 1102`: vars [1], [2], [3]);
// - the level-1 Elyos markers of Poeta (`oracle.py m5d-quests --race ELYOS --map 210010000 --level 1`): the wire ints are
//   `nearby.xmlOnlyWire`, which the oracle lists sorted (1101, 1105, 132180, 132181, 132184, 132199); their wire order is
//   `nearby.xmlOnlyWireOrder.buckets` (exact: one int per bucket, the Java HashMap's iteration order): 1105, 132180, 132181, 132199, 132184,
//   1101. The four with bit 17 are 1108, 1109, 1127 and 1112;
// - quest 1209 "[Spend Coin] Iron", the first of quest_data.xml's 111 extra_category quests (`--quest 1209`: extraCategory COIN_QUEST);
// - level 1's exp need, 400 (player_experience_table.xml:4), and 1101's 130 exp (`--quest 1101`: rewards exp 130).
// No case includes or consults a C++ serverpackets header (m5a-plan.md D9).

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <span>
#include <vector>

#include "decoders/QuestDecoders.h"

#include "NetworkTestSupport.h"

namespace aion::gameserver::scenario::decoders {
namespace {

using network::test::PacketWriter;

/** SM_NEARBY_QUESTS.writeImpl (SM_NEARBY_QUESTS.java:22-31), field by field: the wire ints as given */
std::vector<uint8_t> nearbyBytes(const std::vector<int32_t>& wire) {
	PacketWriter w;
	w.C(0);                                                               // :23
	w.H(static_cast<int32_t>((0x10000u - wire.size()) & 0xFFFFu));       // :24, -size & 0xFFFF
	for (const int32_t value : wire)
		w.D(value); // :29
	return w.data;
}

/**
 * The status byte (offset 5) of an ADD or UPDATE body: QuestStatus.value() 3-6 decode as themselves (SM_QUEST_ACTION.java:76, :83); the
 * ordinals of START, REWARD and COMPLETE (0-2; LOCKED's ordinal 3 is START's value) and 7 are refused (QuestStatus.java:11-14)
 */
void expectTheStatusRange(const std::vector<uint8_t>& body, const char* type) {
	for (const uint8_t status : {QUEST_STATUS_START, QUEST_STATUS_REWARD, QUEST_STATUS_COMPLETE, QUEST_STATUS_LOCKED}) {
		std::vector<uint8_t> changed = body;
		changed[5] = status;
		EXPECT_EQ(decodeQuestAction(changed).status, status) << type << " with QuestStatus.value() " << static_cast<int>(status);
	}
	for (const uint8_t status : {uint8_t{0}, uint8_t{1}, uint8_t{2}, uint8_t{7}}) {
		std::vector<uint8_t> changed = body;
		changed[5] = status;
		EXPECT_THROW(decodeQuestAction(changed), DecodeError) << type << " with status " << static_cast<int>(status);
	}
}

// ---- SM_QUEST_ACTION --------------------------------------------------------------------------------------------------------------------

TEST(QuestDecodersTest, QuestActionAddIsFourteenBytes) {
	// Y3: accepting 1101 at elpas, QuestService.startQuest -> SM_QUEST_ACTION(ADD, qs) with START and vars 0 (QuestService.java:441)
	const std::vector<uint8_t> body = {
		0x01,                   // 0: writeC(actionType.getId()) = ADD           (SM_QUEST_ACTION.java:72)
		0x4D, 0x04, 0x00, 0x00, // 1: writeD(questId) = 1101                     (:73)
		0x03,                   // 5: writeC(status) = START.value()              (:76)
		0x00,                   // 6: writeC(0x0)                                 (:77)
		0x00, 0x00, 0x00, 0x00, // 7: writeD(step | flags << 24) = 0              (:78)
		0x00, 0x00,             // 11: writeH(0)                                  (:79)
		0x00,                   // 13: writeC(0)                                  (:80)
	};
	ASSERT_EQ(body.size(), 14u);
	const QuestAction add = decodeQuestAction(body);
	EXPECT_FALSE(add.empty);
	EXPECT_EQ(add.actionType, QUEST_ACTION_ADD);
	EXPECT_EQ(add.questId, 1101);
	EXPECT_EQ(add.status, QUEST_STATUS_START);
	EXPECT_EQ(add.questVarsAndFlags, 0);

	for (const size_t constant : {size_t{6}, size_t{11}, size_t{12}, size_t{13}}) {
		std::vector<uint8_t> changed = body;
		changed[constant] = 1;
		EXPECT_THROW(decodeQuestAction(changed), DecodeError) << "byte " << constant << " is a Java constant 0";
	}
	expectTheStatusRange(body, "ADD");

	// ADD writes the same vars-and-flags int as UPDATE (:78). QuestService.startQuest sends ADD with the old state when it re-accepts a
	// COMPLETE one (QuestService.java:426-427), and finishQuest clears that state's vars but not its flags (:108-109), so the int need not be
	// 0. This synthetic int tells var 0, var 1 and the flags apart: var 1 = 1 sets bit 6, which a var mask wider than 6 bits would fold into
	// var 0, and the flags 0x12 have bits in both nibbles
	PacketWriter withVars;
	withVars.C(QUEST_ACTION_ADD).D(1101).C(QUEST_STATUS_START).C(0).D(1 | 1 << 6 | 0x12 << 24).H(0).C(0);
	ASSERT_EQ(withVars.data.size(), 14u);
	const QuestAction readded = decodeQuestAction(withVars.data);
	EXPECT_EQ(readded.questVarsAndFlags, 1 | 1 << 6 | 0x12 << 24);
	EXPECT_EQ(readded.var(0), 1);
	EXPECT_EQ(readded.var(1), 1);
	EXPECT_EQ(readded.var(2), 0);
	EXPECT_EQ(readded.highByte(), 0x12) << "step | flags << 24: all eight bits of the flags";

	std::vector<uint8_t> trailing = body;
	trailing.push_back(0);
	EXPECT_THROW(decodeQuestAction(trailing), DecodeError);
	EXPECT_THROW(decodeQuestAction(std::span<const uint8_t>(body).first(13)), DecodeError) << "ADD without its last byte is UPDATE's length";
}

TEST(QuestDecodersTest, QuestActionUpdateIsThirteenBytesAndCarriesTheVarsAndFlags) {
	// Y5: 1101 reported at mires, REWARD with var 0 = 1 (AbstractQuestHandler.java:293 via changeQuestStep / updateQuestStatus)
	const std::vector<uint8_t> body = {
		0x02,                   // 0: writeC(actionType.getId()) = UPDATE        (:72)
		0x4D, 0x04, 0x00, 0x00, // 1: writeD(questId) = 1101                     (:73)
		0x04,                   // 5: writeC(status) = REWARD.value()             (:83)
		0x00,                   // 6: writeC(0x0)                                 (:84)
		0x01, 0x00, 0x00, 0x00, // 7: writeD(step | flags << 24) = var 0 is 1     (:85)
		0x00, 0x00,             // 11: writeH(0)                                  (:86)
	};
	ASSERT_EQ(body.size(), 13u);
	const QuestAction update = decodeQuestAction(body);
	EXPECT_EQ(update.actionType, QUEST_ACTION_UPDATE);
	EXPECT_EQ(update.questId, 1101);
	EXPECT_EQ(update.status, QUEST_STATUS_REWARD);
	EXPECT_EQ(update.var(0), 1);
	EXPECT_EQ(update.var(1), 0);
	EXPECT_EQ(update.highByte(), 0);

	// Y6: finishQuest's UPDATE, COMPLETE with the vars cleared (QuestService.java:112)
	PacketWriter complete;
	complete.C(QUEST_ACTION_UPDATE).D(1101).C(QUEST_STATUS_COMPLETE).C(0).D(0).H(0);
	ASSERT_EQ(complete.data.size(), 13u);
	EXPECT_EQ(decodeQuestAction(complete.data), (QuestAction{false, QUEST_ACTION_UPDATE, 1101, QUEST_STATUS_COMPLETE, 0}));

	// Y8/Y10: 1102's kill count in var 0; var 1 is the next six bits (QuestVars.java:37-47: Sum(var_i * 64^i)) and the flags the top byte
	PacketWriter vars;
	vars.C(QUEST_ACTION_UPDATE).D(1102).C(QUEST_STATUS_START).C(0).D(3 | 2 << 6 | 0x05 << 24).H(0);
	const QuestAction kills = decodeQuestAction(vars.data);
	EXPECT_EQ(kills.var(0), 3);
	EXPECT_EQ(kills.var(1), 2);
	EXPECT_EQ(kills.var(2), 0);
	EXPECT_EQ(kills.highByte(), 0x05) << "step | flags << 24";

	expectTheStatusRange(body, "UPDATE");
	std::vector<uint8_t> changedByte = body;
	changedByte[6] = 1;
	EXPECT_THROW(decodeQuestAction(changedByte), DecodeError) << "UPDATE's writeC(0x0) after the status (:84) is a constant";
	std::vector<uint8_t> changedShort = body;
	changedShort[11] = 1; // "seen sometimes 1 when status == COMPLETED" - but this Java writes 0
	EXPECT_THROW(decodeQuestAction(changedShort), DecodeError);
	std::vector<uint8_t> addsLastByte = body;
	addsLastByte.push_back(0);
	EXPECT_THROW(decodeQuestAction(addsLastByte), DecodeError) << "UPDATE has no fifth field";
}

TEST(QuestDecodersTest, QuestActionAbandonTimerShareAndUnk) {
	// Y12: CM_DELETE_QUEST(1103) -> abandonQuest -> SM_QUEST_ACTION(ABANDON, qs) (QuestService.java:882): the type, the id, writeD(0)
	const std::vector<uint8_t> abandon = {0x03, 0x4F, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; // :72, :73 (1103), :89
	ASSERT_EQ(abandon.size(), 9u);
	EXPECT_EQ(decodeQuestAction(abandon), (QuestAction{false, QUEST_ACTION_ABANDON, 1103}));
	std::vector<uint8_t> abandonNotZero = abandon;
	abandonNotZero[5] = 3; // the status would be here if ABANDON wrote the state
	EXPECT_THROW(decodeQuestAction(abandonNotZero), DecodeError);

	// TIMER: QuestService.questTimerStart sends the seconds (QuestService.java:823; _1044TestingFlightSkills starts 110), and CM_DELETE_QUEST
	// stops a timer quest's with 0 (CM_DELETE_QUEST.java:34); the byte is timer > 0 ? 1 : 0 (:92-93)
	PacketWriter started;
	started.C(QUEST_ACTION_TIMER).D(1044).D(110).C(1);
	ASSERT_EQ(started.data.size(), 10u);
	const QuestAction timer = decodeQuestAction(started.data);
	EXPECT_EQ(timer.actionType, QUEST_ACTION_TIMER);
	EXPECT_EQ(timer.questId, 1044);
	EXPECT_EQ(timer.timer, 110);
	PacketWriter stopped;
	stopped.C(QUEST_ACTION_TIMER).D(1044).D(0).C(0);
	EXPECT_EQ(decodeQuestAction(stopped.data).timer, 0);
	PacketWriter stoppedButRunning;
	stoppedButRunning.C(QUEST_ACTION_TIMER).D(1044).D(0).C(1);
	EXPECT_THROW(decodeQuestAction(stoppedButRunning.data), DecodeError) << "a stopped timer writes 0";
	PacketWriter startedButStopped;
	startedButStopped.C(QUEST_ACTION_TIMER).D(1044).D(110).C(0);
	EXPECT_THROW(decodeQuestAction(startedButStopped.data), DecodeError) << "a running timer writes 1";

	// SHARE: CM_QUEST_SHARE asks a member (CM_QUEST_SHARE.java:78): the sharer and writeD(shareInAlliance ? 1 : 0) (:96-97)
	PacketWriter group;
	group.C(QUEST_ACTION_SHARE).D(1101).D(0x400B0001).D(0);
	ASSERT_EQ(group.data.size(), 13u);
	const QuestAction share = decodeQuestAction(group.data);
	EXPECT_EQ(share.sharerId, 0x400B0001);
	EXPECT_FALSE(share.shareInAlliance);
	PacketWriter alliance;
	alliance.C(QUEST_ACTION_SHARE).D(1101).D(0x400B0001).D(1);
	EXPECT_TRUE(decodeQuestAction(alliance.data).shareInAlliance);
	PacketWriter notABoolean;
	notABoolean.C(QUEST_ACTION_SHARE).D(1101).D(0x400B0001).D(2);
	EXPECT_THROW(decodeQuestAction(notABoolean.data), DecodeError);
	PacketWriter negative;
	negative.C(QUEST_ACTION_SHARE).D(1101).D(0x400B0001).D(-1);
	EXPECT_THROW(decodeQuestAction(negative.data), DecodeError) << "shareInAlliance ? 1 : 0 is never negative";

	// UNK: NpcFactions sends SM_QUEST_ACTION(questId) (NpcFactions.java:238): writeH(0x01), writeH(0x0) (:100-101)
	const std::vector<uint8_t> unk = {0x06, 0x4D, 0x04, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00};
	ASSERT_EQ(unk.size(), 9u);
	EXPECT_EQ(decodeQuestAction(unk), (QuestAction{false, QUEST_ACTION_UNK, 1101}));
	std::vector<uint8_t> unkChanged = unk;
	unkChanged[5] = 0;
	EXPECT_THROW(decodeQuestAction(unkChanged), DecodeError) << "UNK's first short is the constant 1";
	for (const size_t secondShort : {size_t{7}, size_t{8}}) {
		std::vector<uint8_t> changed = unk;
		changed[secondShort] = 1;
		EXPECT_THROW(decodeQuestAction(changed), DecodeError) << "byte " << secondShort << ": UNK's second short is the constant 0 (:101)";
	}

	// no seventh type, and no type 0: ActionType ids are 1-6 (:107-112)
	for (const uint8_t type : {uint8_t{0}, uint8_t{7}}) {
		std::vector<uint8_t> unknown = abandon;
		unknown[0] = type;
		EXPECT_THROW(decodeQuestAction(unknown), DecodeError) << "type " << static_cast<int>(type);
	}
}

TEST(QuestDecodersTest, QuestActionOfAnExtraCategoryQuestIsEmpty) {
	// risk 9: writeImpl returns before its first write for quest 1209 (extra_category COIN_QUEST) and the 110 others (SM_QUEST_ACTION.java:69-71)
	const QuestAction nothing = decodeQuestAction(std::vector<uint8_t>{});
	EXPECT_TRUE(nothing.empty);
	EXPECT_EQ(nothing, (QuestAction{true}));
	// a type byte alone is not the empty body: it is a truncated one
	EXPECT_THROW(decodeQuestAction(std::vector<uint8_t>{QUEST_ACTION_ADD}), DecodeError);
	// the decoder does not look up the quest template: a non-empty body decodes whatever its quest id. Java never writes this one - it writes
	// nothing for 1209, whatever the type (:69-71) - so the gate tells 1209's empty body apart by the `empty` flag, not by the id
	EXPECT_FALSE(decodeQuestAction(std::vector<uint8_t>{0x03, 0xB9, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}).empty)
		<< "an ABANDON body with 1209's id is not the empty body";
}

// ---- SM_NEARBY_QUESTS -------------------------------------------------------------------------------------------------------------------

TEST(QuestDecodersTest, NearbyQuestsOfALevelOneElyosInPoeta) {
	// Y1: the oracle's wire ints (`nearby.xmlOnlyWire`) in the order of `nearby.xmlOnlyWireOrder.buckets`; 1108, 1109, 1127 and 1112 carry
	// bit 17 (their minlevel_permitted is 2 or 3)
	const std::vector<int32_t> wire = {1105, 132180, 132181, 132199, 132184, 1101};
	const std::vector<uint8_t> body = nearbyBytes(wire);
	ASSERT_EQ(body.size(), 3u + 4u * wire.size());
	EXPECT_EQ(body[1], 0xFA) << "-6 & 0xFFFF is 0xFFFA, low byte first";
	EXPECT_EQ(body[2], 0xFF);
	const NearbyQuests nearby = decodeNearbyQuests(body);
	EXPECT_EQ(nearby.ids(), (std::vector<int32_t>{1105, 1108, 1109, 1127, 1112, 1101}));
	EXPECT_EQ(nearby.notYetAvailableIds(), (std::vector<int32_t>{1108, 1109, 1127, 1112}));
	EXPECT_EQ(nearby.wireValues(), wire);
	std::vector<int32_t> sorted = nearby.wireValues();
	std::ranges::sort(sorted);
	EXPECT_EQ(sorted, (std::vector<int32_t>{1101, 1105, 132180, 132181, 132184, 132199})) << "sorted, the oracle's xmlOnlyWire as it lists it";
	EXPECT_TRUE(nearby.contains(1101));
	EXPECT_TRUE(nearby.contains(1127));
	EXPECT_FALSE(nearby.contains(1102)) << "1102 needs 1101 finished";
	EXPECT_FALSE(nearby.contains(132180)) << "contains takes the id without the marker bit";
	ASSERT_EQ(nearby.quests.size(), 6u);
	EXPECT_EQ(nearby.quests[1], (NearbyQuest{1108, true, 132180}));
	EXPECT_EQ(nearby.quests[5], (NearbyQuest{1101, false, 1101}));
}

TEST(QuestDecodersTest, NearbyQuestsEmptyAndMalformed) {
	// before the join no quest is registered: writeC(0), writeH(-0 & 0xFFFF) = 0, and nothing else
	const std::vector<uint8_t> empty = {0x00, 0x00, 0x00};
	EXPECT_TRUE(decodeNearbyQuests(empty).quests.empty());
	EXPECT_EQ(nearbyBytes({}), empty);

	std::vector<uint8_t> leading = nearbyBytes({1101});
	leading[0] = 1;
	EXPECT_THROW(decodeNearbyQuests(leading), DecodeError) << "writeC(0) is a constant";

	PacketWriter positiveCount; // the count must be negated: a positive 1 announces 65535 entries and the body ends early
	positiveCount.C(0).H(1).D(1101);
	EXPECT_THROW(decodeNearbyQuests(positiveCount.data), DecodeError);

	std::vector<uint8_t> trailing = nearbyBytes({1101});
	trailing.push_back(0);
	EXPECT_THROW(decodeNearbyQuests(trailing), DecodeError);
	EXPECT_THROW(decodeNearbyQuests(std::span<const uint8_t>(nearbyBytes({1101, 1105})).first(7)), DecodeError) << "one entry short";

	EXPECT_THROW(decodeNearbyQuests(nearbyBytes({1101, 1101 | NEARBY_QUEST_NOT_YET_AVAILABLE_BIT})), DecodeError)
		<< "a quest id twice: the entries are a Map's keys";
	EXPECT_THROW(decodeNearbyQuests(nearbyBytes({1101 | 1 << 18})), DecodeError) << "a bit above the marker";
	EXPECT_THROW(decodeNearbyQuests(nearbyBytes({-1101})), DecodeError);
	// the bound is exactly 2^18: 2^18 itself is refused, 2^18 - 1 (the marker bit and id 2^17 - 1) is the largest int that decodes
	EXPECT_THROW(decodeNearbyQuests(nearbyBytes({1 << 18})), DecodeError) << "2^18 is the first int above the marker bit's range";
	EXPECT_EQ(decodeNearbyQuests(nearbyBytes({(1 << 18) - 1})).quests[0], (NearbyQuest{(1 << 17) - 1, true, (1 << 18) - 1}));
	// the highest quest id of quest_data.xml, 99002, fits under the marker bit with or without it
	EXPECT_EQ(decodeNearbyQuests(nearbyBytes({99002 | NEARBY_QUEST_NOT_YET_AVAILABLE_BIT})).quests[0], (NearbyQuest{99002, true, 230074}));
}

// ---- SM_STATUPDATE_EXP ------------------------------------------------------------------------------------------------------------------

TEST(QuestDecodersTest, StatUpdateExpIsFiveLongs) {
	// Y6: after 1101's reward a level-1 character shows 130 of 400 (player_experience_table.xml:4), nothing recoverable, no repose energy
	const std::vector<uint8_t> body = {
		0x82, 0, 0, 0, 0, 0, 0, 0, // 0: writeQ(currentExp) = 130      (SM_STATUPDATE_EXP.java:36)
		0x00, 0, 0, 0, 0, 0, 0, 0, // 8: writeQ(recoverableExp) = 0    (:37)
		0x90, 1, 0, 0, 0, 0, 0, 0, // 16: writeQ(maxExp) = 400         (:38)
		0x00, 0, 0, 0, 0, 0, 0, 0, // 24: writeQ(curBoostExp) = 0      (:39)
		0x00, 0, 0, 0, 0, 0, 0, 0, // 32: writeQ(maxBoostExp) = 0      (:40)
	};
	ASSERT_EQ(body.size(), 40u);
	EXPECT_EQ(decodeStatUpdateExp(body), (StatUpdateExp{130, 0, 400, 0, 0}));

	// every field is a long, in this order: the fields are told apart by values that do not fit an int
	PacketWriter wide;
	wide.Q(0x100000001LL).Q(0x200000002LL).Q(0x300000003LL).Q(0x400000004LL).Q(0x500000005LL);
	EXPECT_EQ(decodeStatUpdateExp(wide.data), (StatUpdateExp{0x100000001LL, 0x200000002LL, 0x300000003LL, 0x400000004LL, 0x500000005LL}));

	std::vector<uint8_t> trailing = body;
	trailing.push_back(0);
	EXPECT_THROW(decodeStatUpdateExp(trailing), DecodeError);
	EXPECT_THROW(decodeStatUpdateExp(std::span<const uint8_t>(body).first(32)), DecodeError) << "four longs";
}

} // namespace
} // namespace aion::gameserver::scenario::decoders
