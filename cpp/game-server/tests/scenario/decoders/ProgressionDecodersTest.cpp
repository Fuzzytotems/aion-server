// The G-02 progression decoders (m5e-plan.md §2.11, G-02) against byte vectors written from the Java writeImpl, in the style of
// QuestDecodersTest.cpp: a body built field by field in Java order with the line of each write, the body size Java produces, every
// constant the decoder verifies changed once, and a truncated and an over-long body refused. The values are the gate's own (m5e-plan.md
// §10.3): the class change's CLASS_CHANGE animation at level 9 (X5), the Daeva's 30001 removed (X7, flag 1 of a tapping skill), 519's DP
// (X9), 1699's resurrection offer with 8296 (X13), the Celerity Mantra's 8998 (X14), the training keyblade's robot 2500002 (X15,
// `oracle.py m5e-progression ... --weapon 102100181`), "Invalid class chosen" (X4) and the Fire Spirit's panel (X17).
// No case includes or consults a C++ serverpackets header (m5a-plan.md D9).

#include <gtest/gtest.h>

#include <cstdint>
#include <span>
#include <vector>

#include "decoders/ProgressionDecoders.h"

#include "NetworkTestSupport.h"

namespace aion::gameserver::scenario::decoders {
namespace {

using network::test::PacketWriter;

/** every decoder refuses a body one byte short and one byte long */
template <typename Decode>
void expectExactLength(const std::vector<uint8_t>& body, Decode decode, const char* name) {
	ASSERT_FALSE(body.empty());
	std::vector<uint8_t> shorter(body.begin(), body.end() - 1);
	EXPECT_THROW(decode(shorter), DecodeError) << name << " one byte short";
	std::vector<uint8_t> longer = body;
	longer.push_back(0);
	EXPECT_THROW(decode(longer), DecodeError) << name << " one byte long";
}

TEST(ProgressionDecodersTest, ActionAnimationOfTheClassChange) {
	PacketWriter w;
	w.D(0x12345678);                       // SM_ACTION_ANIMATION.java:28, writeD(targetObjectId)
	w.H(ACTION_ANIMATION_CLASS_CHANGE);    // :29, ActionAnimation.CLASS_CHANGE.getId() = 4
	w.D(9);                                // :30, the level
	ASSERT_EQ(w.data.size(), 10u);
	const ActionAnimation animation = decodeActionAnimation(w.data);
	EXPECT_EQ(animation.objectId, 0x12345678);
	EXPECT_EQ(animation.animation, 4);
	EXPECT_EQ(animation.levelOrObjectId, 9);
	expectExactLength(w.data, [](std::span<const uint8_t> b) { return decodeActionAnimation(b); }, "SM_ACTION_ANIMATION");
}

TEST(ProgressionDecodersTest, SkillRemoveOfTheHumanTapping) {
	PacketWriter w;
	w.H(30001); // SM_SKILL_REMOVE.java:24
	w.C(1);     // :25, getProfessionFlag() of a tapping skill
	w.C(0);     // :26, a normal skill
	ASSERT_EQ(w.data.size(), 4u);
	const SkillRemove remove = decodeSkillRemove(w.data);
	EXPECT_EQ(remove.skillId, 30001);
	EXPECT_EQ(remove.levelOrFlag, 1);
	EXPECT_EQ(remove.skillType, 0);
	for (const uint8_t type : {uint8_t{1}, uint8_t{3}}) {
		std::vector<uint8_t> changed = w.data;
		changed[3] = type;
		EXPECT_EQ(decodeSkillRemove(changed).skillType, type);
	}
	for (const uint8_t type : {uint8_t{2}, uint8_t{4}}) {
		std::vector<uint8_t> changed = w.data;
		changed[3] = type;
		EXPECT_THROW(decodeSkillRemove(changed), DecodeError) << "skill type " << +type;
	}
	expectExactLength(w.data, [](std::span<const uint8_t> b) { return decodeSkillRemove(b); }, "SM_SKILL_REMOVE");
}

TEST(ProgressionDecodersTest, TheTwoDpPackets) {
	PacketWriter dp;
	dp.H(2000); // SM_STATUPDATE_DP.java:24
	ASSERT_EQ(dp.data.size(), 2u);
	EXPECT_EQ(decodeStatUpdateDp(dp.data), 2000);
	expectExactLength(dp.data, [](std::span<const uint8_t> b) { return decodeStatUpdateDp(b); }, "SM_STATUPDATE_DP");

	PacketWriter info;
	info.D(4711); // SM_DP_INFO.java:21
	info.H(0);    // :22, after 519's dpuse of 2,000
	ASSERT_EQ(info.data.size(), 6u);
	EXPECT_EQ(decodeDpInfo(info.data).objectId, 4711);
	EXPECT_EQ(decodeDpInfo(info.data).currentDp, 0);
	expectExactLength(info.data, [](std::span<const uint8_t> b) { return decodeDpInfo(b); }, "SM_DP_INFO");
}

TEST(ProgressionDecodersTest, ResurrectOfferWithTheSkill) {
	PacketWriter w;
	w.S("Gatecleric"); // SM_RESURRECT.java:26, writeS(name)
	w.H(1699);         // :27
	w.D(0);            // :28
	ASSERT_EQ(w.data.size(), (10u + 1u) * 2u + 2u + 4u);
	const Resurrect resurrect = decodeResurrect(w.data);
	EXPECT_EQ(resurrect.name, "Gatecleric");
	EXPECT_EQ(resurrect.skillId, 1699);
	std::vector<uint8_t> changed = w.data;
	changed.back() = 1;
	EXPECT_THROW(decodeResurrect(changed), DecodeError) << "the last int is writeD(0)";
	expectExactLength(w.data, [](std::span<const uint8_t> b) { return decodeResurrect(b); }, "SM_RESURRECT");
}

TEST(ProgressionDecodersTest, MantraEffect) {
	PacketWriter w;
	w.D(0);       // SM_MANTRA_EFFECT.java:22
	w.D(0x4242);  // :23, the effector
	w.H(8998);    // :24, the aura's skill
	ASSERT_EQ(w.data.size(), 10u);
	const MantraEffect mantra = decodeMantraEffect(w.data);
	EXPECT_EQ(mantra.effectorObjectId, 0x4242);
	EXPECT_EQ(mantra.subEffectId, 8998);
	std::vector<uint8_t> changed = w.data;
	changed[0] = 1;
	EXPECT_THROW(decodeMantraEffect(changed), DecodeError) << "the first int is writeD(0x00)";
	expectExactLength(w.data, [](std::span<const uint8_t> b) { return decodeMantraEffect(b); }, "SM_MANTRA_EFFECT");
}

TEST(ProgressionDecodersTest, RideRobotOnAndOff) {
	PacketWriter on;
	on.D(77);      // SM_RIDE_ROBOT.java:26
	on.D(2500002); // :27, the keyblade's robot (item 102100181)
	ASSERT_EQ(on.data.size(), 8u);
	EXPECT_EQ(decodeRideRobot(on.data).objectId, 77);
	EXPECT_EQ(decodeRideRobot(on.data).robotId, 2500002);
	PacketWriter off;
	off.D(77);
	off.D(0); // RideRobotEffect.endEffect: robot id 0
	EXPECT_EQ(decodeRideRobot(off.data).robotId, 0);
	expectExactLength(on.data, [](std::span<const uint8_t> b) { return decodeRideRobot(b); }, "SM_RIDE_ROBOT");
}

TEST(ProgressionDecodersTest, FlyTime) {
	PacketWriter w;
	w.D(55); // SM_FLY_TIME.java:21
	w.D(60); // :22
	ASSERT_EQ(w.data.size(), 8u);
	EXPECT_EQ(decodeFlyTime(w.data).currentFp, 55);
	EXPECT_EQ(decodeFlyTime(w.data).maxFp, 60);
	expectExactLength(w.data, [](std::span<const uint8_t> b) { return decodeFlyTime(b); }, "SM_FLY_TIME");
}

TEST(ProgressionDecodersTest, MessageOfSendMessage) {
	// PacketSendUtility.sendMessage: new SM_MESSAGE(0, null, msg, GOLDEN_YELLOW); writeS(null) is the terminator alone
	PacketWriter w;
	w.C(CHAT_GOLDEN_YELLOW);       // SM_MESSAGE.java:139
	w.C(0);                        // :140, no sender: race filter 0
	w.D(0);                        // :141
	w.H(0);                        // :142, writeS(null)
	w.S("Invalid class chosen");   // :143
	ASSERT_EQ(w.data.size(), 1u + 1u + 4u + 2u + (20u + 1u) * 2u);
	const Message message = decodeMessage(w.data);
	EXPECT_EQ(message.chatType, CHAT_GOLDEN_YELLOW);
	EXPECT_EQ(message.senderObjectId, 0);
	EXPECT_EQ(message.senderName, "");
	EXPECT_EQ(message.message, "Invalid class chosen");
	std::vector<uint8_t> changed = w.data;
	changed[1] = 3;
	EXPECT_THROW(decodeMessage(changed), DecodeError) << "the race filter is 0, 1 or 2";
	expectExactLength(w.data, [](std::span<const uint8_t> b) { return decodeMessage(b); }, "SM_MESSAGE");
}

TEST(ProgressionDecodersTest, MessageShoutCarriesThePosition) {
	PacketWriter w;
	w.C(CHAT_SHOUT);
	w.C(1);
	w.D(99);
	w.S("Npc");
	w.S("hello");
	w.F(1.5f); // SM_MESSAGE.java:145-147
	w.F(2.5f);
	w.F(3.5f);
	const Message message = decodeMessage(w.data);
	EXPECT_EQ(message.senderName, "Npc");
	EXPECT_FLOAT_EQ(message.x, 1.5f);
	EXPECT_FLOAT_EQ(message.z, 3.5f);
	expectExactLength(w.data, [](std::span<const uint8_t> b) { return decodeMessage(b); }, "SM_MESSAGE (SHOUT)");
}

TEST(ProgressionDecodersTest, SummonPanel) {
	PacketWriter w;
	w.D(900001); // SM_SUMMON_PANEL.java:21
	w.H(10);     // :22
	w.D(0);      // :23
	w.D(0);      // :24
	w.D(1500);   // :25
	w.D(1600);   // :26
	w.D(120);    // :27
	w.D(300);    // :28
	w.D(200);    // :29
	w.H(0);      // :30
	w.D(0);      // :31, Summon.getLiveTime()
	ASSERT_EQ(w.data.size(), 40u);
	const SummonPanel panel = decodeSummonPanel(w.data);
	EXPECT_EQ(panel.objectId, 900001);
	EXPECT_EQ(panel.level, 10);
	EXPECT_EQ(panel.currentHp, 1500);
	EXPECT_EQ(panel.maxHp, 1600);
	EXPECT_EQ(panel.mainHandPAttack, 120);
	EXPECT_EQ(panel.pDef, 300);
	EXPECT_EQ(panel.mDef, 200);
	for (const size_t constant : {size_t{6}, size_t{10}, size_t{34}}) {
		std::vector<uint8_t> changed = w.data;
		changed[constant] = 1;
		EXPECT_THROW(decodeSummonPanel(changed), DecodeError) << "offset " << constant;
	}
	expectExactLength(w.data, [](std::span<const uint8_t> b) { return decodeSummonPanel(b); }, "SM_SUMMON_PANEL");
}

TEST(ProgressionDecodersTest, SummonUpdate) {
	PacketWriter w;
	w.C(10).H(1).D(0).D(0).D(1500);                        // SM_SUMMON_UPDATE.java:22-26
	w.D(1600).D(120).D(300).H(40).D(200);                  // :29-41
	w.H(1).H(2).H(3).H(4).H(5).H(6).H(7).H(8);              // :44-65
	w.D(1601).D(121).D(301).H(41).D(201);                  // :67-71
	w.H(11).H(12).H(13).H(14).H(15).H(16).H(17).H(18);      // :72-79
	ASSERT_EQ(w.data.size(), 1u + 2u + 4u * 3u + 4u * 3u + 2u + 4u + 2u * 8u + 4u * 3u + 2u + 4u + 2u * 8u);
	const SummonUpdate update = decodeSummonUpdate(w.data);
	EXPECT_EQ(update.level, 10);
	EXPECT_EQ(update.mode, 1);
	EXPECT_EQ(update.currentHp, 1500);
	EXPECT_EQ(update.mResist, 40);
	EXPECT_EQ(update.evasion, 8);
	EXPECT_EQ(update.baseMaxHp, 1601);
	EXPECT_EQ(update.baseMDef, 201);
	EXPECT_EQ(update.baseEvasion, 18);
	std::vector<uint8_t> changed = w.data;
	changed[3] = 1;
	EXPECT_THROW(decodeSummonUpdate(changed), DecodeError) << "the first unknown int is writeD(0)";
	expectExactLength(w.data, [](std::span<const uint8_t> b) { return decodeSummonUpdate(b); }, "SM_SUMMON_UPDATE");
}

TEST(ProgressionDecodersTest, SummonPanelRemoveAndOwnerRemove) {
	PacketWriter release;
	release.H(0); // SM_SUMMON_PANEL_REMOVE.java:20, SummonsService's release
	release.C(0); // :24
	EXPECT_EQ(decodeSummonPanelRemove(release.data), 0);
	PacketWriter withSkill;
	withSkill.H(3706);
	withSkill.C(1); // :22
	EXPECT_EQ(decodeSummonPanelRemove(withSkill.data), 3706);
	std::vector<uint8_t> wrong = withSkill.data;
	wrong[2] = 0;
	EXPECT_THROW(decodeSummonPanelRemove(wrong), DecodeError) << "skill 3706 writes the flag 1";
	wrong = release.data;
	wrong[2] = 1;
	EXPECT_THROW(decodeSummonPanelRemove(wrong), DecodeError) << "skill 0 writes the flag 0";
	expectExactLength(withSkill.data, [](std::span<const uint8_t> b) { return decodeSummonPanelRemove(b); }, "SM_SUMMON_PANEL_REMOVE");

	PacketWriter owner;
	owner.D(900001); // SM_SUMMON_OWNER_REMOVE.java:19
	EXPECT_EQ(decodeSummonOwnerRemove(owner.data), 900001);
	expectExactLength(owner.data, [](std::span<const uint8_t> b) { return decodeSummonOwnerRemove(b); }, "SM_SUMMON_OWNER_REMOVE");
}

} // namespace
} // namespace aion::gameserver::scenario::decoders
