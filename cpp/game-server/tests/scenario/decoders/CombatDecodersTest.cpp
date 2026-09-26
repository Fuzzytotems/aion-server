// The G-04 combat decoders against hand-built byte vectors (m5b-plan.md §4 G-04, m5a-plan.md D9), in the style of PacketDecodersTest.cpp.
//
// Two kinds of case, and the difference matters:
//   * a **hand-built body** per packet - a literal byte vector with the offset of every field in its comment, written from the Java writeImpl
//     and from nothing else. It is the case that pins the absolute offsets: move a field by one byte in the decoder and the values it reads
//     stop matching, whatever the builders do.
//   * **builder** cases for the branchy packets (SM_ATTACK's shield arms and proc effect, SM_EMOTION's ten switch arms), where the body is
//     written out field by field in Java order and every case also asserts the exact body size Java produces. A decoder that reads a field
//     with the wrong width or in the wrong order cannot pass both the value assertions and the exact-consumption check.
//
// No case here includes or consults a C++ serverpackets header, which is what makes the gate's comparison independent (D9).

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "decoders/CombatDecoders.h"

#include "NetworkTestSupport.h"

namespace aion::gameserver::scenario::decoders {
namespace {

using network::test::PacketWriter;

/** SM_ATTACK.java:44-147, the writeImpl sequence spelled out once more from the Java side */
void attackBody(PacketWriter& writer, const Attack& attack) {
	writer.D(attack.attackerObjectId);     // :45
	writer.C(attack.attackNo);             // :46
	writer.H(attack.time);                 // :47
	writer.C(attack.attackTypeAnimation);  // :48
	writer.C(attack.attackHandAnimation);  // :49
	writer.D(attack.targetObjectId);       // :51
	writer.C(attack.targetHpPercentage);   // :53
	writer.C(attack.attackerHpPercentage); // :54
	writer.H(attack.counterSkillFlag);     // :57-85, the counter skill switch
	writer.H(0);                           // :92
	if (attack.criticalProcPosition) {     // :93-97
		writer.F(attack.criticalProcPosition->x);
		writer.F(attack.criticalProcPosition->y);
		writer.F(attack.criticalProcPosition->z);
	}
	writer.C(static_cast<int32_t>(attack.results.size())); // :105
	for (const AttackResultEntry& result : attack.results) {
		writer.D(result.damage);         // :107
		writer.C(result.attackStatusId); // :108
		writer.C(result.shieldType);     // :110-111
		switch (result.shieldType) {     // :116-144
			case 0:
			case 2:
				break;
			case 8:
			case 10:
				writer.D(result.shield.protectorId).D(result.shield.protectedDamage).D(result.shield.protectedSkillId);
				break;
			case 16:
				writer.D(0).D(0).D(0).D(0).D(0).D(result.shield.mpAbsorbed).D(result.shield.reflectedSkillId);
				break;
			default:
				writer.D(result.shield.protectorId).D(result.shield.protectedDamage).D(result.shield.protectedSkillId);
				writer.D(result.shield.reflectedDamage).D(result.shield.reflectedSkillId).D(0).D(0);
				break;
		}
	}
	writer.C(0); // :146
}

/** SM_EMOTION.java:93-181 */
void emotionBody(PacketWriter& writer, const Emotion& emotion, bool writeOptionalTargetObjectId = true) {
	writer.D(emotion.senderObjectId); // :94
	writer.C(emotion.emotionType);    // :95
	writer.H(emotion.state);          // :96
	writer.F(emotion.speed);          // :97
	switch (emotion.emotionType) {
		case 18: // DIE
		case 40: // START_LOOT
		case 41: // END_LOOT
		case 42: // START_QUESTLOOT
		case 43: // END_QUESTLOOT
			writer.D(emotion.targetObjectId); // :133
			break;
		case 4: // CHAIR_SIT
		case 5: // CHAIR_UP
			writer.F(emotion.x).F(emotion.y).F(emotion.z).C(emotion.heading); // :137-140
			break;
		case 6: // START_FLYTELEPORT
			writer.D(emotion.emotion); // :144
			break;
		case 8: // WINDSTREAM
			writer.D(emotion.emotion).D(emotion.targetObjectId); // :148-149
			break;
		case 15: // RIDE
		case 16: // RIDE_END
			if (writeOptionalTargetObjectId && emotion.targetObjectId != 0)
				writer.D(emotion.targetObjectId); // :153-155
			writer.F(63.0f).F(63.0f).F(64.0f);    // :156-158, writeF(0x3F), writeF(0x3F), writeF(0x40)
			break;
		case 19: // RESURRECT
			writer.D(0); // :162
			break;
		case 21: // EMOTE
			writer.D(emotion.targetObjectId).H(emotion.emotion).C(1); // :166-168
			break;
		case 35: // CHANGE_SPEED
			writer.H(emotion.baseAttackSpeed).H(emotion.currentAttackSpeed).C(0); // :172-174
			break;
		case 17: // ATTACK, and the rest of the default arm
		case 22:
		case 44:
		case 45:
		case 46:
		case 49:
		case 53:
		case 54:
		case 255:
			if (writeOptionalTargetObjectId && emotion.targetObjectId != 0)
				writer.D(emotion.targetObjectId); // :177-179
			break;
		default:
			break; // :99-127, the bare break arms
	}
}

void expectAttackEquals(const Attack& decoded, const Attack& expected) {
	EXPECT_EQ(decoded.attackerObjectId, expected.attackerObjectId);
	EXPECT_EQ(decoded.attackNo, expected.attackNo);
	EXPECT_EQ(decoded.time, expected.time);
	EXPECT_EQ(decoded.attackTypeAnimation, expected.attackTypeAnimation);
	EXPECT_EQ(decoded.attackHandAnimation, expected.attackHandAnimation);
	EXPECT_EQ(decoded.targetObjectId, expected.targetObjectId);
	EXPECT_EQ(decoded.targetHpPercentage, expected.targetHpPercentage);
	EXPECT_EQ(decoded.attackerHpPercentage, expected.attackerHpPercentage);
	EXPECT_EQ(decoded.counterSkillFlag, expected.counterSkillFlag);
	EXPECT_EQ(decoded.criticalProcPosition, expected.criticalProcPosition);
	EXPECT_EQ(decoded.results, expected.results);
}

/** one melee swing of the gate's fight: the player hits npc 210663 for 17 with a critical, no shield, no proc effect */
Attack meleeSwing() {
	Attack attack;
	attack.attackerObjectId = 0x00A13001;
	attack.attackNo = 7;
	attack.time = 750;
	attack.attackTypeAnimation = ATTACK_TYPE_ANIMATION_MELEE;
	attack.attackHandAnimation = ATTACK_HAND_ANIMATION_OFF_HAND;
	attack.targetObjectId = 0x00A13002;
	attack.targetHpPercentage = 88;
	attack.attackerHpPercentage = 100;
	attack.counterSkillFlag = ATTACK_COUNTER_NONE;
	AttackResultEntry hit;
	hit.damage = 17;
	hit.attackStatusId = ATTACK_STATUS_CRITICAL;
	hit.shieldType = 0;
	attack.results.push_back(hit);
	return attack;
}

// ---- SM_ATTACK --------------------------------------------------------------------------------------------------------------------------

TEST(CombatDecodersTest, AttackOfOneMeleeSwingFromAHandBuiltBody) {
	// SM_ATTACK.java:44-147, byte for byte. Offsets are absolute in the body (what writeImpl wrote, after the 5 header bytes).
	const std::vector<uint8_t> body = {
		0x01, 0x30, 0xA1, 0x00, // 0: writeD(attacker.getObjectId()) = 0x00A13001    (:45)
		0x07,                   // 4: writeC(attackno) = 7                           (:46)
		0xEE, 0x02,             // 5: writeH(time) = 750                             (:47)
		0x00,                   // 7: writeC(attackTypeAnimation.getId()) = MELEE     (:48)
		0x01,                   // 8: writeC(attackHandAnimation.getId()) = OFF_HAND  (:49)
		0x02, 0x30, 0xA1, 0x00, // 9: writeD(target.getObjectId()) = 0x00A13002      (:51)
		0x58,                   // 13: writeC(target hp percentage) = 88             (:53)
		0x64,                   // 14: writeC(attacker hp percentage) = 100          (:54)
		0x00, 0x00,             // 15: the counter skill short, default arm, no proc (:82)
		0x00, 0x00,             // 17: writeH(0)                                     (:92)
		0x01,                   // 19: writeC(attackList.size()) = 1                 (:105)
		0x11, 0x00, 0x00, 0x00, // 20: writeD(attack.getDamage()) = 17                (:107)
		0xCA,                   // 24: writeC(AttackStatus.CRITICAL.getId()) = -54    (:108)
		0x00,                   // 25: writeC(shieldType) = 0                         (:111)
		0x00,                   // 26: writeC(0)                                      (:146)
	};
	ASSERT_EQ(body.size(), 27u);

	const Attack decoded = decodeAttack(body);
	EXPECT_EQ(decoded.attackerObjectId, 0x00A13001);
	EXPECT_EQ(decoded.attackNo, 7);
	EXPECT_EQ(decoded.time, 750) << "the delay is a short at offset 5, not an int (SM_ATTACK.java:47)";
	EXPECT_EQ(decoded.attackTypeAnimation, ATTACK_TYPE_ANIMATION_MELEE);
	EXPECT_EQ(decoded.attackHandAnimation, ATTACK_HAND_ANIMATION_OFF_HAND);
	EXPECT_EQ(decoded.targetObjectId, 0x00A13002) << "the target is at offset 9, after attackno, time and the two animation bytes";
	EXPECT_EQ(decoded.targetHpPercentage, 88) << "the TARGET's percentage comes first (SM_ATTACK.java:53)";
	EXPECT_EQ(decoded.attackerHpPercentage, 100) << "the ATTACKER's percentage second (SM_ATTACK.java:54)";
	EXPECT_EQ(decoded.counterSkillFlag, ATTACK_COUNTER_NONE);
	EXPECT_FALSE(decoded.criticalProcPosition.has_value());
	ASSERT_EQ(decoded.results.size(), 1u);
	EXPECT_EQ(decoded.results[0].damage, 17);
	EXPECT_EQ(decoded.results[0].attackStatusId, ATTACK_STATUS_CRITICAL) << "the status byte is signed: 0xCA is -54, not 202";
	EXPECT_EQ(decoded.results[0].shieldType, 0);

	// the same body through the builder, so the builder the branch cases rely on is pinned to the hand-built one
	PacketWriter writer;
	attackBody(writer, meleeSwing());
	EXPECT_EQ(writer.data, body);
}

TEST(CombatDecodersTest, AttackWithACriticalProcEffectCarriesThreeFloats) {
	// SM_ATTACK.java:76-80 and :93-97: the default arm writes 1026 for a non-Player target with a proc effect whose skill is not 8218, and the
	// proc's three target floats follow the writeH(0) of :92.
	Attack expected = meleeSwing();
	expected.counterSkillFlag = ATTACK_PROC_OTHER_TARGET;
	expected.criticalProcPosition = CriticalProcPosition{1226.22f, 1096.57f, 141.93f};
	PacketWriter writer;
	attackBody(writer, expected);
	EXPECT_EQ(writer.data.size(), 39u) << "27 bytes plus the three proc floats";
	expectAttackEquals(decodeAttack(writer.data), expected);

	for (const uint16_t flag : {ATTACK_PROC_PLAYER_TARGET_SKILL_8218, ATTACK_PROC_PLAYER_TARGET, ATTACK_PROC_OTHER_TARGET_SKILL_8218}) {
		Attack variant = expected;
		variant.counterSkillFlag = flag;
		PacketWriter variantWriter;
		attackBody(variantWriter, variant);
		EXPECT_EQ(variantWriter.data.size(), 39u);
		expectAttackEquals(decodeAttack(variantWriter.data), variant);
	}

	// flag 0 is the same arm without a proc effect, and then the floats are not there
	PacketWriter without;
	attackBody(without, meleeSwing());
	EXPECT_EQ(without.data.size(), 27u);
	EXPECT_FALSE(decodeAttack(without.data).criticalProcPosition.has_value());
}

TEST(CombatDecodersTest, AttackCounterFlagsSuppressTheProcFloatsUnlessTheCallerSaysOtherwise) {
	// SM_ATTACK.java:59-74: the four counter arms depend only on the first result's status and never look at the proc effect, so the flag alone
	// cannot say whether the three floats follow. AttackOptions is how the caller states it; the default is no floats.
	for (const uint16_t flag : {ATTACK_COUNTER_BLOCK, ATTACK_COUNTER_PARRY, ATTACK_COUNTER_DODGE, ATTACK_COUNTER_RESIST}) {
		Attack expected = meleeSwing();
		expected.counterSkillFlag = flag;
		expected.results[0].damage = 0;
		expected.results[0].attackStatusId = ATTACK_STATUS_DODGE;
		PacketWriter writer;
		attackBody(writer, expected);
		EXPECT_EQ(writer.data.size(), 27u);
		expectAttackEquals(decodeAttack(writer.data), expected);

		Attack withProc = expected;
		withProc.criticalProcPosition = CriticalProcPosition{1.0f, 2.0f, 3.0f};
		PacketWriter procWriter;
		attackBody(procWriter, withProc);
		EXPECT_EQ(procWriter.data.size(), 39u);
		EXPECT_THROW(decodeAttack(procWriter.data), DecodeError) << "without the option the three floats are read as the result list";
		expectAttackEquals(decodeAttack(procWriter.data, AttackOptions{.criticalProcEffectWithCounterStatus = true}), withProc);
	}
}

TEST(CombatDecodersTest, AttackShieldArmsAndSeveralResults) {
	// SM_ATTACK.java:116-144: each result's tail is chosen by its own shieldType byte, so a packet can mix them.
	Attack expected = meleeSwing();
	expected.results.clear();

	AttackResultEntry plain;
	plain.damage = 41;
	plain.attackStatusId = ATTACK_STATUS_NORMALHIT;
	plain.shieldType = 2; // "normal shield": nothing follows
	expected.results.push_back(plain);

	AttackResultEntry protect;
	protect.damage = 13;
	protect.attackStatusId = ATTACK_STATUS_OFFHAND_NORMALHIT;
	protect.shieldType = 8; // protect effect: protectorId, protectedDamage, protectedSkillId
	protect.shield.protectorId = 0x00A13003;
	protect.shield.protectedDamage = 9;
	protect.shield.protectedSkillId = 417;
	expected.results.push_back(protect);

	AttackResultEntry mpShield;
	mpShield.damage = 5;
	mpShield.attackStatusId = ATTACK_STATUS_BLOCK;
	mpShield.shieldType = 16; // five zero ints, then the absorbed MP and the reflected skill id
	mpShield.shield.mpAbsorbed = 22;
	mpShield.shield.reflectedSkillId = 8218;
	expected.results.push_back(mpShield);

	AttackResultEntry reflector;
	reflector.damage = 3;
	reflector.attackStatusId = ATTACK_STATUS_OFFHAND_CRITICAL;
	reflector.shieldType = 1; // reflector: the default arm, seven ints
	reflector.shield.protectorId = 0x00A13004;
	reflector.shield.protectedDamage = 2;
	reflector.shield.protectedSkillId = 418;
	reflector.shield.reflectedDamage = 6;
	reflector.shield.reflectedSkillId = 419;
	expected.results.push_back(reflector);

	PacketWriter writer;
	attackBody(writer, expected);
	// 19 header bytes, the count byte, then 6 + 18 + 34 + 34 bytes of results, then the closing byte
	EXPECT_EQ(writer.data.size(), 19u + 1u + 6u + 18u + 34u + 34u + 1u);
	expectAttackEquals(decodeAttack(writer.data), expected);
}

TEST(CombatDecodersTest, AttackRejectsAShiftedBodyAChangedConstantAndAnImpossibleList) {
	const Attack swing = meleeSwing();
	PacketWriter good;
	attackBody(good, swing);
	ASSERT_NO_THROW(decodeAttack(good.data));

	// the attack counter is a byte (SM_ATTACK.java:46): widening it to a short shifts every later field
	PacketWriter widenedAttackNo;
	widenedAttackNo.D(swing.attackerObjectId).H(swing.attackNo).H(swing.time).C(0).C(1).D(swing.targetObjectId).C(88).C(100).H(0).H(0);
	widenedAttackNo.C(1).D(17).C(ATTACK_STATUS_CRITICAL).C(0).C(0);
	EXPECT_THROW(decodeAttack(widenedAttackNo.data), DecodeError) << "one byte too many before the target id must not decode";

	// the writeH(0) of :92 carries no data, so a non-zero there is a port that changed a literal
	PacketWriter changedConstant;
	changedConstant.D(swing.attackerObjectId).C(swing.attackNo).H(swing.time).C(0).C(1).D(swing.targetObjectId).C(88).C(100).H(0).H(1);
	changedConstant.C(1).D(17).C(ATTACK_STATUS_CRITICAL).C(0).C(0);
	EXPECT_THROW(decodeAttack(changedConstant.data), DecodeError);

	// so does the writeC(0) of :146
	PacketWriter changedTail;
	attackBody(changedTail, swing);
	changedTail.data.back() = 1;
	EXPECT_THROW(decodeAttack(changedTail.data), DecodeError);

	// the counter skill short can only be one of nine values
	for (const uint16_t flag : {uint16_t{3}, uint16_t{31}, uint16_t{512}, uint16_t{1027}}) {
		Attack variant = swing;
		variant.counterSkillFlag = flag;
		PacketWriter writer;
		attackBody(writer, variant);
		EXPECT_THROW(decodeAttack(writer.data), DecodeError) << "counter skill short " << flag;
	}

	// SM_ATTACK.java:57 reads attackList.get(0) before writing anything, so an empty list cannot come from Java
	PacketWriter emptyList;
	emptyList.D(swing.attackerObjectId).C(swing.attackNo).H(swing.time).C(0).C(1).D(swing.targetObjectId).C(88).C(100).H(0).H(0).C(0).C(0);
	EXPECT_THROW(decodeAttack(emptyList.data), DecodeError);

	// D9: the body is consumed exactly
	PacketWriter trailing;
	attackBody(trailing, swing);
	trailing.C(0);
	EXPECT_THROW(decodeAttack(trailing.data), DecodeError);
	PacketWriter truncated;
	attackBody(truncated, swing);
	truncated.data.pop_back();
	EXPECT_THROW(decodeAttack(truncated.data), DecodeError);

	// the five zero ints of the shieldType 16 arm are literals too (SM_ATTACK.java:127-131)
	Attack mpShield = swing;
	mpShield.results[0].shieldType = 16;
	mpShield.results[0].shield.mpAbsorbed = 22;
	mpShield.results[0].shield.reflectedSkillId = 8218;
	PacketWriter shieldWriter;
	attackBody(shieldWriter, mpShield);
	ASSERT_EQ(shieldWriter.data.size(), 55u);
	ASSERT_NO_THROW(decodeAttack(shieldWriter.data));
	shieldWriter.data[26] = 1; // the first byte of the first of the five zero ints, which start right after the shieldType byte at 25
	EXPECT_THROW(decodeAttack(shieldWriter.data), DecodeError);

	// and so are the two zero ints that close the default shield arm (SM_ATTACK.java:141-142)
	Attack reflector = swing;
	reflector.results[0].shieldType = 1;
	reflector.results[0].shield.protectorId = 0x00A13004;
	reflector.results[0].shield.protectedDamage = 2;
	reflector.results[0].shield.protectedSkillId = 418;
	reflector.results[0].shield.reflectedDamage = 6;
	reflector.results[0].shield.reflectedSkillId = 419;
	PacketWriter reflectorWriter;
	attackBody(reflectorWriter, reflector);
	ASSERT_EQ(reflectorWriter.data.size(), 55u);
	ASSERT_NO_THROW(decodeAttack(reflectorWriter.data));
	reflectorWriter.data[46] = 1; // the five written ints end at 45, so the first of the two zero ints starts at 46
	EXPECT_THROW(decodeAttack(reflectorWriter.data), DecodeError);
}

// ---- SM_ATTACK_STATUS -------------------------------------------------------------------------------------------------------------------

TEST(CombatDecodersTest, AttackStatusFromAHandBuiltBody) {
	// SM_ATTACK_STATUS.java:122-158, the packet A3 and A6 read. TYPE.REGULAR takes the default arm of :149-151, so the damage is positive.
	const std::vector<uint8_t> body = {
		0x02, 0x30, 0xA1, 0x00, // 0: writeD(creature.getObjectId()) = 0x00A13002  (:124)
		0x11, 0x00, 0x00, 0x00, // 4: writeD(value) = 17, the default arm          (:150)
		0x05,                   // 8: writeC(type.getValue()) = REGULAR            (:153)
		0x58,                   // 9: writeC(hpOrMp) = 88 %                        (:154)
		0x00, 0x00,             // 10: writeH(skillId) = 0                         (:155)
		0xBF,                   // 12: writeC(logId) = LOG.REGULAR = 191           (:156)
		0x0C,                   // 13: writeC(CRITICAL_DISPLAY_CODE) = 12          (:157)
	};
	ASSERT_EQ(body.size(), 14u);

	const AttackStatusUpdate decoded = decodeAttackStatus(body);
	EXPECT_EQ(decoded.creatureObjectId, 0x00A13002);
	EXPECT_EQ(decoded.value, 17);
	EXPECT_EQ(decoded.type, ATTACK_STATUS_TYPE_REGULAR);
	EXPECT_EQ(decoded.hpOrMp, 88) << "the percentage follows the type byte (SM_ATTACK_STATUS.java:153-154)";
	EXPECT_EQ(decoded.skillId, 0);
	EXPECT_EQ(decoded.logId, ATTACK_STATUS_LOG_REGULAR);
	EXPECT_TRUE(decoded.criticalHit);
}

TEST(CombatDecodersTest, AttackStatusOfRegenerationAndOfANegatedDamageType) {
	// A6 (iii): the character's HP regeneration during a fight is TYPE.NATURAL_HP, which also takes the default arm, so its value is positive
	PacketWriter regen;
	regen.D(0x00A13001).D(9).C(ATTACK_STATUS_TYPE_NATURAL_HP).C(94).H(0).C(ATTACK_STATUS_LOG_REGULAR).C(0);
	EXPECT_EQ(regen.data.size(), 14u);
	const AttackStatusUpdate decodedRegen = decodeAttackStatus(regen.data);
	EXPECT_EQ(decodedRegen.type, ATTACK_STATUS_TYPE_NATURAL_HP);
	EXPECT_EQ(decodedRegen.value, 9);
	EXPECT_EQ(decodedRegen.hpOrMp, 94);
	EXPECT_FALSE(decodedRegen.criticalHit);

	// TYPE.DAMAGE is one of the arms of :126-136 that writes -value, so the decoder must read the int signed
	PacketWriter spellDamage;
	spellDamage.D(0x00A13001).D(-42).C(ATTACK_STATUS_TYPE_DAMAGE).C(70).H(1234).C(1).C(0);
	const AttackStatusUpdate decodedDamage = decodeAttackStatus(spellDamage.data);
	EXPECT_EQ(decodedDamage.value, -42);
	EXPECT_EQ(decodedDamage.skillId, 1234) << "the skill id is a short between the percentage and the log byte";
	EXPECT_EQ(decodedDamage.logId, 1);
}

TEST(CombatDecodersTest, AttackStatusRejectsAWrongCriticalCodeAndAnyOtherLength) {
	PacketWriter wrongCritical;
	wrongCritical.D(0x00A13002).D(17).C(ATTACK_STATUS_TYPE_REGULAR).C(88).H(0).C(ATTACK_STATUS_LOG_REGULAR).C(1);
	EXPECT_THROW(decodeAttackStatus(wrongCritical.data), DecodeError) << "SM_ATTACK_STATUS.java:157 writes only 0 or 12";

	PacketWriter trailing;
	trailing.D(0x00A13002).D(17).C(ATTACK_STATUS_TYPE_REGULAR).C(88).H(0).C(ATTACK_STATUS_LOG_REGULAR).C(0).C(0);
	EXPECT_THROW(decodeAttackStatus(trailing.data), DecodeError);

	PacketWriter truncated;
	truncated.D(0x00A13002).D(17).C(ATTACK_STATUS_TYPE_REGULAR).C(88).H(0).C(ATTACK_STATUS_LOG_REGULAR);
	EXPECT_THROW(decodeAttackStatus(truncated.data), DecodeError);
}

// ---- SM_ATTACK_RESPONSE -----------------------------------------------------------------------------------------------------------------

TEST(CombatDecodersTest, AttackResponseFromAHandBuiltBodyAndEveryFactoryMessage) {
	// SM_ATTACK_RESPONSE.java:45-48: two bytes, the message first (A1a reads TARGET_TOO_FAR_AWAY, A2 STOP_WITHOUT_MESSAGE)
	const std::vector<uint8_t> body = {
		0x04, // 0: writeC(message) = TARGET_TOO_FAR_AWAY  (:46)
		0x03, // 1: writeC(attackCount) = 3                (:47)
	};
	const AttackResponse decoded = decodeAttackResponse(body);
	EXPECT_EQ(decoded.message, ATTACK_RESPONSE_TARGET_TOO_FAR_AWAY);
	EXPECT_EQ(decoded.attackCount, 3);

	for (const uint8_t message : {ATTACK_RESPONSE_TARGET_IN_DIFFERENT_AREA, ATTACK_RESPONSE_STOP_INVALID_TARGET,
			 ATTACK_RESPONSE_TARGET_TOO_FAR_AWAY, ATTACK_RESPONSE_STOP_OBSTACLE_IN_THE_WAY, ATTACK_RESPONSE_STOP_TOO_CLOSE_TO_ATTACK,
			 ATTACK_RESPONSE_STOP_WITHOUT_MESSAGE}) {
		PacketWriter writer;
		writer.C(message).C(200);
		EXPECT_EQ(writer.data.size(), 2u);
		const AttackResponse response = decodeAttackResponse(writer.data);
		EXPECT_EQ(response.message, message);
		EXPECT_EQ(response.attackCount, 200) << "the counter is a byte, so Java's int wraps at 256";
	}

	// there is no factory for 0 or 3, so they cannot come from a correct server
	for (const uint8_t message : {uint8_t{0}, uint8_t{3}, uint8_t{8}}) {
		PacketWriter writer;
		writer.C(message).C(1);
		EXPECT_THROW(decodeAttackResponse(writer.data), DecodeError) << "message " << int{message};
	}

	PacketWriter trailing;
	trailing.C(ATTACK_RESPONSE_TARGET_TOO_FAR_AWAY).C(1).C(0);
	EXPECT_THROW(decodeAttackResponse(trailing.data), DecodeError);
}

// ---- SM_EMOTION -------------------------------------------------------------------------------------------------------------------------

TEST(CombatDecodersTest, EmotionDieFromAHandBuiltBody) {
	// SM_EMOTION.java:94-97 and :128-134. D1a: the last attacker is the FIFTH field on the wire, after the speed float, not the `emotion` one.
	const std::vector<uint8_t> body = {
		0x02, 0x30, 0xA1, 0x00, // 0: writeD(senderObjectId) = the monster            (:94)
		0x12,                   // 4: writeC(EmotionType.DIE.getTypeId()) = 18        (:95)
		0x40, 0x00,             // 5: writeH(state) = 0x40                            (:96)
		0x00, 0x00, 0xC0, 0x3F, // 7: writeF(speed) = 1.5                             (:97)
		0x01, 0x30, 0xA1, 0x00, // 11: writeD(targetObjectId) = the player            (:133)
	};
	ASSERT_EQ(body.size(), 15u);

	const Emotion decoded = decodeEmotion(body);
	EXPECT_EQ(decoded.senderObjectId, 0x00A13002);
	EXPECT_EQ(decoded.emotionType, EMOTION_DIE);
	EXPECT_EQ(decoded.state, 0x40);
	EXPECT_FLOAT_EQ(decoded.speed, 1.5f);
	EXPECT_EQ(decoded.targetObjectId, 0x00A13001) << "the last attacker sits at offset 11, right after the speed float";
	EXPECT_EQ(decoded.emotion, 0) << "the DIE arm never writes the `emotion` field";

	// the self-kill case: CreatureController.java:164-165 passes 0 when the creature killed itself
	PacketWriter selfKill;
	selfKill.D(0x00A13002).C(EMOTION_DIE).H(0x40).F(1.5f).D(0);
	EXPECT_EQ(decodeEmotion(selfKill.data).targetObjectId, 0);
}

TEST(CombatDecodersTest, EmotionEmptyArmsAreTheHeaderAndNothingElse) {
	// SM_EMOTION.java:99-127: 28 of the 55 types have a bare break, so their body is exactly the 11 header bytes
	for (const uint8_t type : {uint8_t{0}, uint8_t{3}, uint8_t{7}, uint8_t{13}, EMOTION_ATTACKMODE_IN_MOVE, EMOTION_NEUTRALMODE_IN_MOVE,
			 EMOTION_WALK, uint8_t{27}, uint8_t{31}, uint8_t{38}, uint8_t{47}, uint8_t{52}}) {
		Emotion expected;
		expected.senderObjectId = 0x00A13001;
		expected.emotionType = type;
		expected.state = 0x41;
		expected.speed = 6.0f;
		PacketWriter writer;
		emotionBody(writer, expected);
		EXPECT_EQ(writer.data.size(), 11u) << "emotion type " << int{type};
		const Emotion decoded = decodeEmotion(writer.data);
		EXPECT_EQ(decoded.emotionType, type);
		EXPECT_EQ(decoded.senderObjectId, 0x00A13001);
		EXPECT_EQ(decoded.state, 0x41);
		EXPECT_FLOAT_EQ(decoded.speed, 6.0f);

		writer.C(0);
		EXPECT_THROW(decodeEmotion(writer.data), DecodeError) << "a payload after an empty arm is a framing error, type " << int{type};
	}
}

TEST(CombatDecodersTest, EmotionArmsWithAPayload) {
	Emotion base;
	base.senderObjectId = 0x00A13001;
	base.state = 0x40;
	base.speed = 6.0f;

	// the four loot emotions share the DIE arm (SM_EMOTION.java:128-134)
	for (const uint8_t type : {uint8_t{40}, uint8_t{41}, uint8_t{42}, uint8_t{43}}) {
		Emotion expected = base;
		expected.emotionType = type;
		expected.targetObjectId = 0x00A13007;
		PacketWriter writer;
		emotionBody(writer, expected);
		EXPECT_EQ(writer.data.size(), 15u);
		EXPECT_EQ(decodeEmotion(writer.data).targetObjectId, 0x00A13007);
	}

	// CHAIR_SIT / CHAIR_UP: three floats and a heading byte (:135-141)
	for (const uint8_t type : {uint8_t{4}, uint8_t{5}}) {
		Emotion expected = base;
		expected.emotionType = type;
		expected.x = 1212.94f;
		expected.y = 1044.85f;
		expected.z = 140.76f;
		expected.heading = 77;
		PacketWriter writer;
		emotionBody(writer, expected);
		EXPECT_EQ(writer.data.size(), 24u);
		const Emotion decoded = decodeEmotion(writer.data);
		EXPECT_FLOAT_EQ(decoded.x, 1212.94f);
		EXPECT_FLOAT_EQ(decoded.y, 1044.85f);
		EXPECT_FLOAT_EQ(decoded.z, 140.76f);
		EXPECT_EQ(decoded.heading, 77);
		EXPECT_EQ(decoded.targetObjectId, 0) << "the chair arm writes no target id";
	}

	// START_FLYTELEPORT writes the teleport id into `emotion` (:142-145)
	Emotion flyTeleport = base;
	flyTeleport.emotionType = 6;
	flyTeleport.emotion = 331;
	PacketWriter flyWriter;
	emotionBody(flyWriter, flyTeleport);
	EXPECT_EQ(flyWriter.data.size(), 15u);
	EXPECT_EQ(decodeEmotion(flyWriter.data).emotion, 331);
	EXPECT_EQ(decodeEmotion(flyWriter.data).targetObjectId, 0);

	// WINDSTREAM writes the teleport id and then the distance (:146-150)
	Emotion windstream = base;
	windstream.emotionType = 8;
	windstream.emotion = 331;
	windstream.targetObjectId = 4200;
	PacketWriter windWriter;
	emotionBody(windWriter, windstream);
	EXPECT_EQ(windWriter.data.size(), 19u);
	const Emotion decodedWind = decodeEmotion(windWriter.data);
	EXPECT_EQ(decodedWind.emotion, 331);
	EXPECT_EQ(decodedWind.targetObjectId, 4200) << "the distance follows the teleport id (SM_EMOTION.java:148-149)";

	// RIDE / RIDE_END: the ride id only when it is not 0, then writeF(0x3F), writeF(0x3F), writeF(0x40) (:151-159)
	for (const uint8_t type : {uint8_t{15}, uint8_t{16}}) {
		Emotion withRideId = base;
		withRideId.emotionType = type;
		withRideId.targetObjectId = 0x00A13009;
		PacketWriter withWriter;
		emotionBody(withWriter, withRideId);
		EXPECT_EQ(withWriter.data.size(), 27u);
		EXPECT_EQ(decodeEmotion(withWriter.data).targetObjectId, 0x00A13009);

		Emotion withoutRideId = base;
		withoutRideId.emotionType = type;
		PacketWriter withoutWriter;
		emotionBody(withoutWriter, withoutRideId);
		EXPECT_EQ(withoutWriter.data.size(), 23u);
		EXPECT_EQ(decodeEmotion(withoutWriter.data).targetObjectId, 0);

		// the three floats are literals: 0x3F and 0x40 are ints converted to 63.0f and 64.0f, not bit patterns
		PacketWriter wrongConstant;
		emotionBody(wrongConstant, withoutRideId);
		wrongConstant.data.back() = 0x41; // the high byte of the last float: 64.0f becomes 16.0f
		EXPECT_THROW(decodeEmotion(wrongConstant.data), DecodeError);
	}

	// RESURRECT writes a literal zero int (:160-163)
	Emotion resurrect = base;
	resurrect.emotionType = 19;
	PacketWriter resurrectWriter;
	emotionBody(resurrectWriter, resurrect);
	EXPECT_EQ(resurrectWriter.data.size(), 15u);
	EXPECT_NO_THROW(decodeEmotion(resurrectWriter.data));
	resurrectWriter.data.back() = 1;
	EXPECT_THROW(decodeEmotion(resurrectWriter.data), DecodeError);

	// EMOTE: the target, the emote id as a short, and a literal 1 (:164-169)
	Emotion emote = base;
	emote.emotionType = 21;
	emote.targetObjectId = 0x00A13008;
	emote.emotion = 20;
	PacketWriter emoteWriter;
	emotionBody(emoteWriter, emote);
	EXPECT_EQ(emoteWriter.data.size(), 18u);
	const Emotion decodedEmote = decodeEmotion(emoteWriter.data);
	EXPECT_EQ(decodedEmote.targetObjectId, 0x00A13008);
	EXPECT_EQ(decodedEmote.emotion, 20);
	emoteWriter.data.back() = 0;
	EXPECT_THROW(decodeEmotion(emoteWriter.data), DecodeError) << "the closing byte of the EMOTE arm is a literal 1";

	// CHANGE_SPEED: the two attack speeds and a literal 0 (:170-175)
	Emotion changeSpeed = base;
	changeSpeed.emotionType = EMOTION_CHANGE_SPEED;
	changeSpeed.baseAttackSpeed = 1500;
	changeSpeed.currentAttackSpeed = 1300;
	PacketWriter speedWriter;
	emotionBody(speedWriter, changeSpeed);
	EXPECT_EQ(speedWriter.data.size(), 16u);
	const Emotion decodedSpeed = decodeEmotion(speedWriter.data);
	EXPECT_EQ(decodedSpeed.baseAttackSpeed, 1500) << "the base speed comes first (SM_EMOTION.java:172-173)";
	EXPECT_EQ(decodedSpeed.currentAttackSpeed, 1300);

	// the default arm writes the target id only when it is not 0 (:176-179)
	for (const uint8_t type : {uint8_t{17}, uint8_t{22}, uint8_t{44}, uint8_t{49}, uint8_t{54}, uint8_t{255}}) {
		Emotion withTarget = base;
		withTarget.emotionType = type;
		withTarget.targetObjectId = 0x00A1300A;
		PacketWriter withWriter;
		emotionBody(withWriter, withTarget);
		EXPECT_EQ(withWriter.data.size(), 15u) << "emotion type " << int{type};
		EXPECT_EQ(decodeEmotion(withWriter.data).targetObjectId, 0x00A1300A);

		Emotion withoutTarget = base;
		withoutTarget.emotionType = type;
		PacketWriter withoutWriter;
		emotionBody(withoutWriter, withoutTarget);
		EXPECT_EQ(withoutWriter.data.size(), 11u);
		EXPECT_EQ(decodeEmotion(withoutWriter.data).targetObjectId, 0);
	}
}

TEST(CombatDecodersTest, EmotionRejectsATypeThatIsNotAnEmotionTypeIdAndAnyExtraBytes) {
	// 20, 23, 28, 29 and 30 are the gaps model/EmotionType.java:6-62 leaves, and 55 and up are not ids either
	for (const uint8_t type : {uint8_t{20}, uint8_t{23}, uint8_t{28}, uint8_t{30}, uint8_t{55}, uint8_t{200}}) {
		PacketWriter writer;
		writer.D(0x00A13001).C(type).H(0).F(1.0f);
		EXPECT_THROW(decodeEmotion(writer.data), DecodeError) << "emotion type " << int{type};
	}

	PacketWriter dieWithTrailing;
	dieWithTrailing.D(0x00A13002).C(EMOTION_DIE).H(0).F(1.0f).D(7).C(0);
	EXPECT_THROW(decodeEmotion(dieWithTrailing.data), DecodeError);

	PacketWriter dieTruncated;
	dieTruncated.D(0x00A13002).C(EMOTION_DIE).H(0).F(1.0f).H(7);
	EXPECT_THROW(decodeEmotion(dieTruncated.data), DecodeError);

	// the RIDE arm is 12 or 16 bytes; anything else is a framing error and must not be guessed at
	PacketWriter rideWrongLength;
	rideWrongLength.D(0x00A13001).C(15).H(0).F(1.0f).H(3).F(63.0f).F(63.0f).F(64.0f);
	EXPECT_THROW(decodeEmotion(rideWrongLength.data), DecodeError);
}

// ---- SM_DIE, SM_LOOT_STATUS and SM_STATUPDATE_HP ----------------------------------------------------------------------------------------

TEST(CombatDecodersTest, DieFromAHandBuiltBody) {
	// SM_DIE.java:31-37, the packet P1 waits for
	const std::vector<uint8_t> body = {
		0x00,                   // 0: writeC(allowReviveBySkill ? 1 : 0)      (:32)
		0x01,                   // 1: writeC(allowReviveByItem ? 1 : 0)       (:33)
		0x2C, 0x01, 0x00, 0x00, // 2: writeD(remainingKiskTimeSeconds) = 300  (:34)
		0x00,                   // 6: writeC(allowInstanceRevive ? 1 : 0)     (:35)
		0x80,                   // 7: writeC(invasion ? 0x80 : 0x00)          (:36)
	};
	ASSERT_EQ(body.size(), 8u);

	const Die decoded = decodeDie(body);
	EXPECT_FALSE(decoded.allowReviveBySkill);
	EXPECT_TRUE(decoded.allowReviveByItem) << "the item flag is the SECOND byte (SM_DIE.java:32-33)";
	EXPECT_EQ(decoded.remainingKiskTimeSeconds, 300);
	EXPECT_FALSE(decoded.allowInstanceRevive) << "0 selects ReviveType.BIND_REVIVE, which is what P2 sends CM_REVIVE for";
	EXPECT_TRUE(decoded.invasion);

	// the gate's own case: no kisk, no self revive, not an invasion world
	PacketWriter plain;
	plain.C(0).C(0).D(0).C(0).C(0);
	const Die plainDeath = decodeDie(plain.data);
	EXPECT_EQ(plainDeath.remainingKiskTimeSeconds, 0);
	EXPECT_FALSE(plainDeath.invasion);

	for (const std::vector<uint8_t> bad : {std::vector<uint8_t>{2, 0, 0, 0, 0, 0, 0, 0}, std::vector<uint8_t>{0, 2, 0, 0, 0, 0, 0, 0},
			 std::vector<uint8_t>{0, 0, 0, 0, 0, 0, 2, 0}, std::vector<uint8_t>{0, 0, 0, 0, 0, 0, 0, 1},
			 std::vector<uint8_t>{0, 0, 0, 0, 0, 0, 0, 0x80, 0}})
		EXPECT_THROW(decodeDie(bad), DecodeError);
}

TEST(CombatDecodersTest, LootStatusFromAHandBuiltBody) {
	// SM_LOOT_STATUS.java:27-31; R3 becomes an assertion on this packet when M5b-3 lands
	const std::vector<uint8_t> body = {
		0x02, 0x30, 0xA1, 0x00, // 0: writeD(targetObjectId) = the corpse      (:28)
		0x00,                   // 4: writeC(Status.LOOT_ENABLE.getId()) = 0   (:29)
		0x15, 0x00, 0x00, 0x00, // 5: writeD(lootEffectId) = 21                (:30)
	};
	ASSERT_EQ(body.size(), 9u);

	const LootStatus decoded = decodeLootStatus(body);
	EXPECT_EQ(decoded.targetObjectId, 0x00A13002);
	EXPECT_EQ(decoded.status, LOOT_STATUS_LOOT_ENABLE);
	EXPECT_EQ(decoded.lootEffectId, 21) << "the loot effect is an int after the status byte, not before it";

	for (const uint8_t status : {LOOT_STATUS_LOOT_DISABLE, LOOT_STATUS_OPEN_DROP_LIST, LOOT_STATUS_CLOSE_DROP_LIST}) {
		PacketWriter writer;
		writer.D(0x00A13002).C(status).D(0);
		EXPECT_EQ(decodeLootStatus(writer.data).status, status);
	}

	PacketWriter unknownStatus;
	unknownStatus.D(0x00A13002).C(4).D(0);
	EXPECT_THROW(decodeLootStatus(unknownStatus.data), DecodeError) << "SM_LOOT_STATUS.java:38-53 declares four statuses";

	PacketWriter trailing;
	trailing.D(0x00A13002).C(LOOT_STATUS_LOOT_ENABLE).D(0).C(0);
	EXPECT_THROW(decodeLootStatus(trailing.data), DecodeError);
}

TEST(CombatDecodersTest, StatUpdateHpFromAHandBuiltBody) {
	// SM_STATUPDATE_HP.java:26-29: two absolute ints, current first. A6 (i) reconstructs the HP and compares it against this packet.
	const std::vector<uint8_t> body = {
		0x89, 0x00, 0x00, 0x00, // 0: writeD(currentHp) = 137  (:27)
		0xC7, 0x00, 0x00, 0x00, // 4: writeD(maxHp) = 199      (:28)
	};
	ASSERT_EQ(body.size(), 8u);

	const StatUpdateHp decoded = decodeStatUpdateHp(body);
	EXPECT_EQ(decoded.currentHp, 137) << "the current value comes first (SM_STATUPDATE_HP.java:27-28)";
	EXPECT_EQ(decoded.maxHp, 199);

	PacketWriter dead;
	dead.D(0).D(199);
	EXPECT_EQ(decodeStatUpdateHp(dead.data).currentHp, 0);

	PacketWriter trailing;
	trailing.D(137).D(199).C(0);
	EXPECT_THROW(decodeStatUpdateHp(trailing.data), DecodeError);
	PacketWriter truncated;
	truncated.D(137).H(199);
	EXPECT_THROW(decodeStatUpdateHp(truncated.data), DecodeError);
}

} // namespace
} // namespace aion::gameserver::scenario::decoders
