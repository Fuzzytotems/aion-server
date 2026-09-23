// The G-02 skill decoders against hand-built byte vectors (m5b2-plan.md §5 G-02, m5a-plan.md D9), in the style of CombatDecodersTest.cpp.
//
// Two kinds of case, and the difference matters:
//   * a **hand-built body** per packet - a literal byte vector with the offset of every field in its comment, written from the Java writeImpl
//     and from nothing else. It is the case that pins the absolute offsets: move a field by one byte in the decoder and the values it reads
//     stop matching, whatever the builders do.
//   * **builder** cases for the branchy packets (SM_CASTSPELL's target arms, SM_CASTSPELL_RESULT's item, dash, spell status and shield arms,
//     SM_ABNORMAL_EFFECT's two effect types), where the body is written out field by field in Java order and every case also asserts the exact
//     body size Java produces. A decoder that reads a field with the wrong width or in the wrong order cannot pass both the value assertions
//     and the exact-consumption check.
//
// No case here includes or consults a C++ serverpackets header, which is what makes the gate's comparison independent (D9).

#include <gtest/gtest.h>

#include <algorithm>
#include <bit>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <span>
#include <vector>

#include "decoders/SkillDecoders.h"

#include "NetworkTestSupport.h"

namespace aion::gameserver::scenario::decoders {
namespace {

using network::test::PacketWriter;

// ---- the Java writeImpl sequences, spelled out once more from the Java side ----------------------------------------------------------------

/** SM_CASTSPELL.java:47-78 */
void castSpellBody(PacketWriter& writer, const CastSpell& cast) {
	writer.D(cast.effectorObjectId).H(cast.spellId).C(cast.level).C(cast.targetType); // :47-50
	switch (cast.targetType) {
		case 0:
		case 3:
		case 4:
			writer.D(cast.targetObjectId); // :55
			break;
		case 1:
			writer.F(cast.targetPosition->x).F(cast.targetPosition->y).F(cast.targetPosition->z); // :58-60
			break;
		case 2:
			writer.F(cast.targetPosition->x).F(cast.targetPosition->y).F(cast.targetPosition->z); // :63-65
			for (int i = 0; i < 8; i++)
				writer.D(0); // :66-73
			break;
		default:
			break;
	}
	writer.H(cast.castDuration).C(0).F(cast.castSpeed).C(cast.allowAnimationBoostByCastSpeed ? 1 : 0); // :75-78
}

/** SM_CASTSPELL_RESULT.java:52-210 */
void castSpellResultBody(PacketWriter& writer, const CastSpellResult& result, const std::vector<size_t>& moveSubEffects = {}) {
	writer.D(result.effectorObjectId).C(result.targetType); // :52-53
	switch (result.targetType) {
		case 0:
		case 3:
		case 4:
			writer.D(result.targetObjectId); // :58
			break;
		case 1:
			writer.F(result.targetPosition->x).F(result.targetPosition->y).F(result.targetPosition->z); // :61-63
			break;
		case 2:
			writer.F(result.targetPosition->x).F(result.targetPosition->y).F(result.targetPosition->z); // :66-68
			for (int i = 0; i < 8; i++)
				writer.F(0.0f); // :69-76
			break;
		default:
			break;
	}
	writer.H(result.skillId).C(result.skillTemplateLevel).D(result.cooldown).H(result.hitTime).C(0); // :79-83
	writer.C(result.effects.empty() ? 16 : result.chainStatus);                                     // :89-94
	if (result.itemArm) {
		writer.C(2).D(result.itemObjectId).D(result.itemTemplateId).C(0); // :97-100
	} else {
		writer.C(result.penaltySkill ? 4 : 0).C(result.dashStatus); // :102-106
		switch (result.dashStatus) {
			case 1:
			case 2:
			case 3:
			case 4:
			case 6:
				writer.C(*result.dashHeading).F(result.dashPosition->x).F(result.dashPosition->y).F(result.dashPosition->z); // :113-116
				break;
			default:
				break;
		}
	}
	writer.H(static_cast<int32_t>(result.effects.size())); // :121
	for (size_t i = 0; i < result.effects.size(); i++) {
		const CastResultEffect& effect = result.effects[i];
		writer.D(effect.effectedObjectId).C(effect.effectResult).C(effect.targetHpPercentage); // :126-128
		writer.C(effect.effectorHpPercentage).C(effect.spellStatus).C(effect.successfulEffects).H(0).C(effect.carvedSignet); // :135-143
		switch (effect.spellStatus) {
			case 1:
			case 2:
			case 4:
			case 8:
				writer.F(effect.targetPosition->x).F(effect.targetPosition->y).F(effect.targetPosition->z); // :149-151
				break;
			case 16:
				writer.C(*effect.effectorHeading); // :154
				break;
			default:
				if (std::find(moveSubEffects.begin(), moveSubEffects.end(), i) != moveSubEffects.end())
					writer.F(effect.targetPosition->x).F(effect.targetPosition->y).F(effect.targetPosition->z); // :161-163
				break;
		}
		writer.C(static_cast<int32_t>(effect.reserved.size())); // :170
		for (const CastResultReserved& reserved : effect.reserved) {
			writer.C(reserved.resourceType).D(reserved.value).C(reserved.attackStatusId).C(reserved.shieldDefense); // :172-188
			switch (reserved.shieldDefense) {
				case 0:
				case 2:
					break;
				case 8:
				case 10:
					writer.D(reserved.protectorId).D(reserved.protectedDamage).D(reserved.protectedSkillId); // :195-197
					break;
				default:
					writer.D(reserved.protectorId).D(reserved.protectedDamage).D(reserved.protectedSkillId); // :200-202
					writer.D(reserved.reflectedDamage).D(reserved.reflectedSkillId).D(reserved.mpAbsorbed).D(reserved.mpShieldSkillId); // :203-206
					break;
			}
		}
	}
}

/** SM_ABNORMAL_EFFECT.java:40-61 for effect types 1 and 2 */
void abnormalEffectBody(PacketWriter& writer, const AbnormalEffect& effect) {
	writer.D(effect.effectedObjectId).C(effect.effectType).D(0).D(effect.abnormals).D(0).C(effect.slots); // :40-45
	writer.H(static_cast<int32_t>(effect.effects.size()));                                                // :46
	for (const AbnormalEntry& entry : effect.effects) {
		if (effect.effectType == 2)
			writer.D(*entry.effectorObjectId); // :50
		writer.H(entry.skillId).C(entry.skillLevel).C(entry.targetSlotOrdinal).D(entry.remainingTimeToDisplay); // :52-55
	}
}

/** 2864 Ferocious Strike landing on npc 210663 for 42: the gate's X2/X3 shape (a chain success, one effect, one HP reservation) */
CastSpellResult ferociousStrike() {
	CastSpellResult result;
	result.effectorObjectId = 0x00A13001;
	result.targetType = CAST_TARGET_OBJECT;
	result.targetObjectId = 0x00A13002;
	result.skillId = 2864;
	result.skillTemplateLevel = 1;
	result.cooldown = 100;
	result.hitTime = 750;
	result.chainStatus = CAST_RESULT_CHAIN_SUCCESS;
	CastResultEffect effect;
	effect.effectedObjectId = 0x00A13002;
	effect.effectResult = EFFECT_RESULT_NORMAL;
	effect.targetHpPercentage = 79;
	effect.effectorHpPercentage = 100;
	effect.spellStatus = SPELL_STATUS_NONE_OR_RESIST;
	effect.successfulEffects = 1;
	CastResultReserved hp;
	hp.resourceType = RESERVED_RESOURCE_HP;
	hp.value = 42;
	hp.attackStatusId = 10; // AttackStatus.NORMALHIT
	effect.reserved.push_back(hp);
	result.effects.push_back(effect);
	return result;
}

void expectResultEquals(const CastSpellResult& decoded, const CastSpellResult& expected) {
	EXPECT_EQ(decoded.effectorObjectId, expected.effectorObjectId);
	EXPECT_EQ(decoded.targetType, expected.targetType);
	EXPECT_EQ(decoded.targetObjectId, expected.targetObjectId);
	EXPECT_EQ(decoded.targetPosition, expected.targetPosition);
	EXPECT_EQ(decoded.skillId, expected.skillId);
	EXPECT_EQ(decoded.skillTemplateLevel, expected.skillTemplateLevel);
	EXPECT_EQ(decoded.cooldown, expected.cooldown);
	EXPECT_EQ(decoded.hitTime, expected.hitTime);
	EXPECT_EQ(decoded.chainStatus, expected.effects.empty() ? CAST_RESULT_NO_EFFECT : expected.chainStatus);
	EXPECT_EQ(decoded.itemArm, expected.itemArm);
	EXPECT_EQ(decoded.itemObjectId, expected.itemObjectId);
	EXPECT_EQ(decoded.itemTemplateId, expected.itemTemplateId);
	EXPECT_EQ(decoded.penaltySkill, expected.penaltySkill);
	EXPECT_EQ(decoded.dashStatus, expected.dashStatus);
	EXPECT_EQ(decoded.dashHeading, expected.dashHeading);
	EXPECT_EQ(decoded.dashPosition, expected.dashPosition);
	EXPECT_EQ(decoded.effects, expected.effects);
}

// ---- SM_CASTSPELL -----------------------------------------------------------------------------------------------------------------------

TEST(SkillDecodersTest, CastSpellOfFlameBoltFromAHandBuiltBody) {
	// SM_CASTSPELL.java:47-78, byte for byte: the Mage's 1282 Flame Bolt on npc 210663, a 2,000 ms cast bar (X4)
	const std::vector<uint8_t> body = {
		0x01, 0x30, 0xA1, 0x00, // 0: writeD(effector.getObjectId()) = 0x00A13001        (:47)
		0x02, 0x05,             // 4: writeH(spellId) = 1282                             (:48)
		0x01,                   // 6: writeC(level) = 1                                  (:49)
		0x00,                   // 7: writeC(targetType) = 0                             (:50)
		0x02, 0x30, 0xA1, 0x00, // 8: writeD(targetObjectId) = 0x00A13002                (:55)
		0xD0, 0x07,             // 12: writeH(castDuration) = 2000                        (:75)
		0x00,                   // 14: writeC(0x00)                                       (:76)
		0x00, 0x00, 0x80, 0x3F, // 15: writeF(castSpeed) = 1.0f                           (:77)
		0x01,                   // 19: writeC(allowAnimationBoostByCastSpeed ? 1 : 0) = 1 (:78)
	};
	ASSERT_EQ(body.size(), 20u);

	const CastSpell cast = decodeCastSpell(body);
	EXPECT_EQ(cast.effectorObjectId, 0x00A13001);
	EXPECT_EQ(cast.spellId, 1282) << "a short at offset 4";
	EXPECT_EQ(cast.level, 1) << "the level comes before the target type (SM_CASTSPELL.java:49-50)";
	EXPECT_EQ(cast.targetType, CAST_TARGET_OBJECT);
	EXPECT_EQ(cast.targetObjectId, 0x00A13002) << "the object id arm at offset 8";
	EXPECT_FALSE(cast.targetPosition.has_value());
	EXPECT_EQ(cast.castDuration, 2000) << "the cast bar is a short at offset 12 (SM_CASTSPELL.java:75)";
	EXPECT_EQ(std::bit_cast<uint32_t>(cast.castSpeed), std::bit_cast<uint32_t>(1.0f)) << "the cast speed float at offset 15, after the zero byte";
	EXPECT_TRUE(cast.allowAnimationBoostByCastSpeed);

	PacketWriter writer;
	castSpellBody(writer, cast);
	EXPECT_EQ(writer.data, body) << "the builder the branch cases rely on is pinned to the hand-built body";
}

TEST(SkillDecodersTest, CastSpellTargetArms) {
	CastSpell base;
	base.effectorObjectId = 7;
	base.spellId = 1328;
	base.level = 3;
	base.castDuration = 0;
	base.castSpeed = 0.75f;
	for (const uint8_t type : {CAST_TARGET_OBJECT_NOT_IN_SIGHT, CAST_TARGET_OBJECT_4}) {
		CastSpell cast = base;
		cast.targetType = type;
		cast.targetObjectId = 0x01020304;
		PacketWriter writer;
		castSpellBody(writer, cast);
		EXPECT_EQ(writer.data.size(), 20u);
		EXPECT_EQ(decodeCastSpell(writer.data).targetObjectId, 0x01020304) << "type " << int{type} << " is an object id arm (:52-56)";
	}
	CastSpell point = base;
	point.targetType = CAST_TARGET_POINT;
	point.targetPosition = SkillPosition{1226.22f, 1096.57f, 141.93f};
	PacketWriter pointWriter;
	castSpellBody(pointWriter, point);
	EXPECT_EQ(pointWriter.data.size(), 28u) << "three floats instead of one int";
	EXPECT_EQ(decodeCastSpell(pointWriter.data).targetPosition, point.targetPosition);
	EXPECT_EQ(decodeCastSpell(pointWriter.data).targetObjectId, 0);

	CastSpell extended = point;
	extended.targetType = CAST_TARGET_POINT_EXTENDED;
	PacketWriter extendedWriter;
	castSpellBody(extendedWriter, extended);
	EXPECT_EQ(extendedWriter.data.size(), 60u) << "the three floats and eight zero ints of :62-73";
	const CastSpell decodedExtended = decodeCastSpell(extendedWriter.data);
	EXPECT_EQ(decodedExtended.targetPosition, point.targetPosition);
	EXPECT_EQ(decodedExtended.castSpeed, 0.75f);
	std::vector<uint8_t> nonZeroFiller = extendedWriter.data;
	nonZeroFiller[8 + 12 + 31] = 1; // the last byte of the eighth zero word
	EXPECT_THROW(decodeCastSpell(nonZeroFiller), DecodeError) << "the zero words are verified, not skipped";

	CastSpell none = base;
	none.targetType = 9;
	PacketWriter noneWriter;
	castSpellBody(noneWriter, none);
	EXPECT_EQ(noneWriter.data.size(), 16u) << "a target type without an arm writes nothing: the switch has no default";
	EXPECT_EQ(decodeCastSpell(noneWriter.data).targetType, 9);
}

TEST(SkillDecodersTest, CastSpellRejectsWhatJavaCannotWrite) {
	CastSpell cast;
	cast.targetType = CAST_TARGET_OBJECT;
	cast.castDuration = 2000;
	cast.castSpeed = 1.0f;
	PacketWriter writer;
	castSpellBody(writer, cast);
	ASSERT_NO_THROW(decodeCastSpell(writer.data));

	std::vector<uint8_t> unknownByte = writer.data;
	unknownByte[14] = 5;
	EXPECT_THROW(decodeCastSpell(unknownByte), DecodeError) << "SM_CASTSPELL.java:76 writes a literal 0";
	std::vector<uint8_t> flag = writer.data;
	flag[19] = 2;
	EXPECT_THROW(decodeCastSpell(flag), DecodeError) << "the animation boost byte is a boolean";
	std::vector<uint8_t> trailing = writer.data;
	trailing.push_back(0);
	EXPECT_THROW(decodeCastSpell(trailing), DecodeError) << "the body is consumed exactly";
	EXPECT_THROW(decodeCastSpell(std::span<const uint8_t>(writer.data).first(19)), DecodeError);
}

TEST(SkillDecodersTest, CastSpellDurationIsAShort) {
	// writeH(castDuration) truncates: a 70,000 ms cast bar reaches the client as 70000 - 65536 = 4464 (m5b2-plan.md §2.7)
	const std::vector<uint8_t> body = PacketWriter().D(1).H(1).C(1).C(0).D(2).H(70000).C(0).F(1.0f).C(0).data;
	EXPECT_EQ(decodeCastSpell(body).castDuration, 4464);
}

// ---- SM_CASTSPELL_RESULT ----------------------------------------------------------------------------------------------------------------

TEST(SkillDecodersTest, CastSpellResultOfFerociousStrikeFromAHandBuiltBody) {
	// SM_CASTSPELL_RESULT.java:52-210, byte for byte: the Warrior's 2864 on npc 210663, a chain success with one effect and one HP reservation
	const std::vector<uint8_t> body = {
		0x01, 0x30, 0xA1, 0x00, // 0: writeD(effector.getObjectId()) = 0x00A13001                  (:52)
		0x00,                   // 4: writeC(targetType) = 0                                       (:53)
		0x02, 0x30, 0xA1, 0x00, // 5: writeD(target.getObjectId()) = 0x00A13002                    (:58)
		0x30, 0x0B,             // 9: writeH(skillId) = 2864                                        (:79)
		0x01,                   // 11: writeC(getLvl()) = 1                                         (:80)
		0x64, 0x00, 0x00, 0x00, // 12: writeD(cooldown) = 100                                       (:81)
		0xEE, 0x02,             // 16: writeH(hitTime) = 750                                        (:82)
		0x00,                   // 18: writeC(0)                                                    (:83)
		0x20,                   // 19: writeC(32), a chain success                                  (:92)
		0x00,                   // 20: writeC(0), not a penalty skill                               (:105)
		0x00,                   // 21: writeC(dashStatus) = 0                                       (:106)
		0x01, 0x00,             // 22: writeH(effects.size()) = 1                                   (:121)
		0x02, 0x30, 0xA1, 0x00, // 24: writeD(effected.getObjectId()) = 0x00A13002                  (:126)
		0x00,                   // 28: writeC(effectResult) = NORMAL                                (:127)
		0x4F,                   // 29: writeC(target %hp) = 79                                      (:128)
		0x64,                   // 30: writeC(attacker %hp) = 100                                   (:135)
		0x00,                   // 31: writeC(spellStatus) = NONE                                   (:140)
		0x01,                   // 32: writeC(successfulEffects) = 1                                (:141)
		0x00, 0x00,             // 33: writeH(0)                                                    (:142)
		0x00,                   // 35: writeC(carvedSignet) = 0                                     (:143)
		0x01,                   // 36: writeC(reservedEffects.size()) = 1                           (:170)
		0x00,                   // 37: writeC(ResourceType.HP) = 0                                  (:172)
		0x2A, 0x00, 0x00, 0x00, // 38: writeD(valueToSend) = 42                                     (:173)
		0x0A,                   // 42: writeC(AttackStatus.NORMALHIT) = 10                          (:174)
		0x00,                   // 43: writeC(shieldDefense) = 0                                    (:188)
	};
	ASSERT_EQ(body.size(), 44u);

	const CastSpellResult result = decodeCastSpellResult(body);
	EXPECT_EQ(result.effectorObjectId, 0x00A13001);
	EXPECT_EQ(result.targetType, CAST_TARGET_OBJECT);
	EXPECT_EQ(result.targetObjectId, 0x00A13002);
	EXPECT_EQ(result.skillId, 2864) << "a short at offset 9";
	EXPECT_EQ(result.skillTemplateLevel, 1);
	EXPECT_EQ(result.cooldown, 100) << "an INT at offset 12 (SM_CASTSPELL_RESULT.java:81), unlike the shorts around it";
	EXPECT_EQ(result.hitTime, 750) << "a short at offset 16";
	EXPECT_EQ(result.chainStatus, CAST_RESULT_CHAIN_SUCCESS) << "offset 19, after the zero byte of :83 (X3)";
	EXPECT_FALSE(result.itemArm);
	EXPECT_FALSE(result.penaltySkill);
	EXPECT_EQ(result.dashStatus, 0);
	ASSERT_EQ(result.effects.size(), 1u);
	const CastResultEffect& effect = result.effects[0];
	EXPECT_EQ(effect.effectedObjectId, 0x00A13002);
	EXPECT_EQ(effect.effectResult, EFFECT_RESULT_NORMAL);
	EXPECT_EQ(effect.targetHpPercentage, 79) << "the TARGET's percentage at offset 29";
	EXPECT_EQ(effect.effectorHpPercentage, 100) << "the EFFECTOR's at offset 30";
	EXPECT_EQ(effect.spellStatus, SPELL_STATUS_NONE_OR_RESIST);
	EXPECT_EQ(effect.successfulEffects, 1);
	EXPECT_EQ(effect.carvedSignet, 0);
	EXPECT_FALSE(effect.targetPosition.has_value());
	EXPECT_FALSE(effect.effectorHeading.has_value());
	ASSERT_EQ(effect.reserved.size(), 1u);
	EXPECT_EQ(effect.reserved[0].resourceType, RESERVED_RESOURCE_HP);
	EXPECT_EQ(effect.reserved[0].value, 42) << "the damage the reservation sends, an int at offset 38";
	EXPECT_EQ(effect.reserved[0].attackStatusId, 10);
	EXPECT_EQ(effect.reserved[0].shieldDefense, 0);

	PacketWriter writer;
	castSpellResultBody(writer, ferociousStrike());
	EXPECT_EQ(writer.data, body) << "the builder the branch cases rely on is pinned to the hand-built body";
}

TEST(SkillDecodersTest, CastSpellResultChainStatusFollowsTheEffectList) {
	// SM_CASTSPELL_RESULT.java:89-94: 16 exactly when the list is empty, which is what X2 reads as "no target was hit"
	CastSpellResult empty = ferociousStrike();
	empty.effects.clear();
	PacketWriter emptyWriter;
	castSpellResultBody(emptyWriter, empty);
	EXPECT_EQ(emptyWriter.data.size(), 24u) << "the header, the arm bytes and a zero effect count";
	EXPECT_EQ(decodeCastSpellResult(emptyWriter.data).chainStatus, CAST_RESULT_NO_EFFECT);

	CastSpellResult plain = ferociousStrike();
	plain.chainStatus = CAST_RESULT_NO_CHAIN;
	PacketWriter plainWriter;
	castSpellResultBody(plainWriter, plain);
	EXPECT_EQ(decodeCastSpellResult(plainWriter.data).chainStatus, CAST_RESULT_NO_CHAIN);

	std::vector<uint8_t> sixteenWithEffects = plainWriter.data;
	sixteenWithEffects[19] = CAST_RESULT_NO_EFFECT;
	EXPECT_THROW(decodeCastSpellResult(sixteenWithEffects), DecodeError) << "16 with a non-empty list cannot come from :89-94";
	std::vector<uint8_t> zeroWithoutEffects = emptyWriter.data;
	zeroWithoutEffects[19] = CAST_RESULT_NO_CHAIN;
	EXPECT_THROW(decodeCastSpellResult(zeroWithoutEffects), DecodeError) << "an empty list always writes 16";
	std::vector<uint8_t> other = plainWriter.data;
	other[19] = 5;
	EXPECT_THROW(decodeCastSpellResult(other), DecodeError);
	std::vector<uint8_t> unknownByte = plainWriter.data;
	unknownByte[18] = 1;
	EXPECT_THROW(decodeCastSpellResult(unknownByte), DecodeError) << "SM_CASTSPELL_RESULT.java:83 writes a literal 0";
}

TEST(SkillDecodersTest, CastSpellResultItemPenaltyAndDashArms) {
	CastSpellResult item = ferociousStrike();
	item.itemArm = true;
	item.itemObjectId = 0x00B00001;
	item.itemTemplateId = 160000001;
	PacketWriter itemWriter;
	castSpellResultBody(itemWriter, item);
	EXPECT_EQ(itemWriter.data.size(), 44u + 8u) << "the item arm is 10 bytes instead of 2 (:97-100)";
	expectResultEquals(decodeCastSpellResult(itemWriter.data), item);
	std::vector<uint8_t> itemTail = itemWriter.data;
	itemTail[20 + 9] = 1;
	EXPECT_THROW(decodeCastSpellResult(itemTail), DecodeError) << "the item arm closes with a literal 0 (:100)";

	CastSpellResult penalty = ferociousStrike();
	penalty.penaltySkill = true;
	PacketWriter penaltyWriter;
	castSpellResultBody(penaltyWriter, penalty);
	EXPECT_EQ(penaltyWriter.data[20], CAST_RESULT_PENALTY_SKILL_ARM);
	expectResultEquals(decodeCastSpellResult(penaltyWriter.data), penalty);
	std::vector<uint8_t> otherArm = penaltyWriter.data;
	otherArm[20] = 1;
	EXPECT_THROW(decodeCastSpellResult(otherArm), DecodeError) << "only 2, 4 and 0 open an arm";

	for (const uint8_t dash : std::initializer_list<uint8_t>{1, 2, 3, 4, 6}) {
		CastSpellResult dashing = ferociousStrike();
		dashing.dashStatus = dash;
		dashing.dashHeading = 60;
		dashing.dashPosition = SkillPosition{1.5f, 2.5f, 3.5f};
		PacketWriter writer;
		castSpellResultBody(writer, dashing);
		EXPECT_EQ(writer.data.size(), 44u + 13u) << "dash status " << int{dash} << " writes the heading and three floats (:113-116)";
		expectResultEquals(decodeCastSpellResult(writer.data), dashing);
	}
	CastSpellResult notDashing = ferociousStrike();
	notDashing.dashStatus = 5;
	PacketWriter notDashingWriter;
	castSpellResultBody(notDashingWriter, notDashing);
	EXPECT_EQ(notDashingWriter.data.size(), 44u) << "dash status 5 is not one of the arm's values";
	expectResultEquals(decodeCastSpellResult(notDashingWriter.data), notDashing);
}

TEST(SkillDecodersTest, CastSpellResultTargetArms) {
	CastSpellResult point = ferociousStrike();
	point.targetType = CAST_TARGET_POINT;
	point.targetObjectId = 0;
	point.targetPosition = SkillPosition{10.0f, 20.0f, 30.0f};
	PacketWriter pointWriter;
	castSpellResultBody(pointWriter, point);
	EXPECT_EQ(pointWriter.data.size(), 44u + 8u);
	expectResultEquals(decodeCastSpellResult(pointWriter.data), point);

	CastSpellResult extended = point;
	extended.targetType = CAST_TARGET_POINT_EXTENDED;
	PacketWriter extendedWriter;
	castSpellResultBody(extendedWriter, extended);
	EXPECT_EQ(extendedWriter.data.size(), 44u + 8u + 32u) << "eight writeF(0) after the point (:69-76)";
	expectResultEquals(decodeCastSpellResult(extendedWriter.data), extended);

	CastSpellResult none = ferociousStrike();
	none.targetType = 7;
	none.targetObjectId = 0;
	PacketWriter noneWriter;
	castSpellResultBody(noneWriter, none);
	EXPECT_EQ(noneWriter.data.size(), 40u) << "no arm at all";
	expectResultEquals(decodeCastSpellResult(noneWriter.data), none);
}

TEST(SkillDecodersTest, CastSpellResultSpellStatusArms) {
	for (const uint8_t status : {SPELL_STATUS_STUMBLE, SPELL_STATUS_STAGGER, SPELL_STATUS_OPENAERIAL, SPELL_STATUS_CLOSEAERIAL}) {
		CastSpellResult result = ferociousStrike();
		result.effects[0].spellStatus = status;
		result.effects[0].targetPosition = SkillPosition{4.0f, 5.0f, 6.0f};
		PacketWriter writer;
		castSpellResultBody(writer, result);
		EXPECT_EQ(writer.data.size(), 44u + 12u) << "spell status " << int{status} << " writes the target position (:145-152)";
		expectResultEquals(decodeCastSpellResult(writer.data), result);
	}
	CastSpellResult spin = ferociousStrike();
	spin.effects[0].spellStatus = SPELL_STATUS_SPIN;
	spin.effects[0].effectorHeading = 90;
	PacketWriter spinWriter;
	castSpellResultBody(spinWriter, spin);
	EXPECT_EQ(spinWriter.data.size(), 45u) << "SPIN writes one heading byte (:153-155)";
	expectResultEquals(decodeCastSpellResult(spinWriter.data), spin);

	for (const uint8_t status : {SPELL_STATUS_BLOCK, SPELL_STATUS_PARRY, SPELL_STATUS_DODGE}) {
		CastSpellResult result = ferociousStrike();
		result.effects[0].spellStatus = status;
		PacketWriter writer;
		castSpellResultBody(writer, result);
		EXPECT_EQ(writer.data.size(), 44u) << "spell status " << int{status} << " takes the default arm";
		expectResultEquals(decodeCastSpellResult(writer.data), result);
	}
	// a byte that is no SpellStatus id, in a body whose layout the default arm would otherwise read cleanly
	PacketWriter plainWriter;
	castSpellResultBody(plainWriter, ferociousStrike());
	for (const uint8_t status : std::initializer_list<uint8_t>{3, 5, 128 + 16, 255}) {
		std::vector<uint8_t> notAStatus = plainWriter.data;
		notAStatus[31] = status;
		EXPECT_THROW(decodeCastSpellResult(notAStatus), DecodeError) << int{status} << " is no SpellStatus id as writeC truncates them";
	}
	std::vector<uint8_t> zeroShort = spinWriter.data;
	zeroShort[34] = 1;
	EXPECT_THROW(decodeCastSpellResult(zeroShort), DecodeError) << "SM_CASTSPELL_RESULT.java:142 writes a literal short 0";
	std::vector<uint8_t> result8 = spinWriter.data;
	result8[28] = 7;
	EXPECT_THROW(decodeCastSpellResult(result8), DecodeError) << "7 is no EffectResult id";
}

TEST(SkillDecodersTest, CastSpellResultMoveSubEffectsAreTheCallersToName) {
	// :156-165: in the default arm the position follows only for a PULL / PULL_NPC / SIMPLE_MOVE_BACK sub effect, which the bytes do not carry
	CastSpellResult result = ferociousStrike();
	result.effects.push_back(result.effects[0]);
	result.effects[1].effectedObjectId = 0x00A13003;
	result.effects[1].targetPosition = SkillPosition{7.0f, 8.0f, 9.0f};
	PacketWriter writer;
	castSpellResultBody(writer, result, {1});
	EXPECT_EQ(writer.data.size(), 44u + 20u + 12u) << "a second effect of 20 bytes, and its position";
	expectResultEquals(decodeCastSpellResult(writer.data, CastSpellResultOptions{.effectsWithMoveSubEffect = {1}}), result);
	EXPECT_THROW(decodeCastSpellResult(writer.data), DecodeError) << "without the option the position is read as the reservation list";
	EXPECT_THROW(decodeCastSpellResult(writer.data, CastSpellResultOptions{.effectsWithMoveSubEffect = {0}}), DecodeError)
		<< "the option names the effect by its index";
}

TEST(SkillDecodersTest, CastSpellResultReservationShieldArms) {
	CastSpellResult result = ferociousStrike();
	CastResultReserved& first = result.effects[0].reserved[0];
	first.shieldDefense = 2; // a normal shield: nothing follows (:190-192)
	CastResultReserved protect;
	protect.resourceType = RESERVED_RESOURCE_MP;
	protect.value = -5;
	protect.attackStatusId = -54; // AttackStatus.CRITICAL, written as 0xCA
	protect.shieldDefense = 8;
	protect.protectorId = 11;
	protect.protectedDamage = 12;
	protect.protectedSkillId = 417;
	CastResultReserved reflector = protect;
	reflector.resourceType = RESERVED_RESOURCE_DP;
	reflector.shieldDefense = 1; // the default arm, seven ints (:200-206)
	reflector.reflectedDamage = 13;
	reflector.reflectedSkillId = 14;
	reflector.mpAbsorbed = 15;
	reflector.mpShieldSkillId = 16;
	CastResultReserved protect10 = protect;
	protect10.shieldDefense = 10;
	CastResultReserved mpShield = reflector;
	mpShield.shieldDefense = 16; // unlike SM_ATTACK, 16 has no arm of its own here: the default one
	result.effects[0].reserved = {first, protect, reflector, protect10, mpShield};
	PacketWriter writer;
	castSpellResultBody(writer, result);
	EXPECT_EQ(writer.data.size(), 36u + 1u + 5u * 7u + 12u + 28u + 12u + 28u) << "the count, five 7-byte heads, and the 0, 12, 28, 12, 28 tails";
	expectResultEquals(decodeCastSpellResult(writer.data), result);

	std::vector<uint8_t> resource = writer.data;
	resource[37] = 4;
	EXPECT_THROW(decodeCastSpellResult(resource), DecodeError) << "4 is no EffectReserved.ResourceType value";
}

// ---- SM_SKILL_CANCEL --------------------------------------------------------------------------------------------------------------------

TEST(SkillDecodersTest, SkillCancelFromAHandBuiltBody) {
	const std::vector<uint8_t> body = {
		0x01, 0x30, 0xA1, 0x00, // 0: writeD(creature.getObjectId()) = 0x00A13001 (SM_SKILL_CANCEL.java:22)
		0x02, 0x05,             // 4: writeH(skillId) = 1282                        (:23)
	};
	const SkillCancel cancel = decodeSkillCancel(body);
	EXPECT_EQ(cancel.creatureObjectId, 0x00A13001);
	EXPECT_EQ(cancel.skillId, 1282);
	std::vector<uint8_t> longer = body;
	longer.push_back(0);
	EXPECT_THROW(decodeSkillCancel(longer), DecodeError);
	EXPECT_THROW(decodeSkillCancel(std::span<const uint8_t>(body).first(5)), DecodeError);
}

// ---- SM_ABNORMAL_STATE ------------------------------------------------------------------------------------------------------------------

TEST(SkillDecodersTest, AbnormalStateFromAHandBuiltBody) {
	// SM_ABNORMAL_STATE.java:26-38: the character's own icons - 3195's two BUFF entries (X7) and the soul sickness at death count 2 (X10)
	const std::vector<uint8_t> body = {
		0x40, 0x00, 0x00, 0x00, // 0: writeD(abnormals) = 0x40                     (:26)
		0x00, 0x00, 0x00, 0x00, // 4: writeD(0)                                    (:27)
		0x00, 0x00, 0x00, 0x00, // 8: writeD(0)                                    (:28)
		0x7F,                   // 12: writeC(slot) = FULLSLOTS                     (:29)
		0x03, 0x00,             // 13: writeH(effects.size()) = 3                   (:30)
		0x01, 0x30, 0xA1, 0x00, // 15: writeD(effectorId) = 0x00A13001              (:33)
		0x7B, 0x0C,             // 19: writeH(skillId) = 3195                       (:34)
		0x01,                   // 21: writeC(skillLevel) = 1                       (:35)
		0x00,                   // 22: writeC(targetSlot.ordinal()) = BUFF          (:36)
		0x88, 0x13, 0x00, 0x00, // 23: writeD(remainingTimeToDisplay) = 5000        (:37)
		0x01, 0x30, 0xA1, 0x00, // 27: the second entry of the same effect
		0x7B, 0x0C,             // 31
		0x01,                   // 33
		0x00,                   // 34
		0x87, 0x13, 0x00, 0x00, // 35: 4999
		0x01, 0x30, 0xA1, 0x00, // 39: the soul sickness: effector and effected are the character
		0x63, 0x20,             // 43: 8291
		0x02,                   // 45: skill level = death count 2
		0x04,                   // 46: SPEC2
		0xFF, 0xFF, 0xFF, 0xFF, // 47: -1, a permanent effect
	};
	ASSERT_EQ(body.size(), 51u);
	const AbnormalState state = decodeAbnormalState(body);
	EXPECT_EQ(state.abnormals, 0x40);
	EXPECT_EQ(state.slot, TARGET_SLOT_FULLSLOTS) << "the slot byte at offset 12, after THREE ints";
	ASSERT_EQ(state.effects.size(), 3u);
	EXPECT_EQ(state.effects[0], (AbnormalEntry{0x00A13001, 3195, 1, TARGET_SLOT_ORDINAL_BUFF, 5000}));
	EXPECT_EQ(state.effects[1], (AbnormalEntry{0x00A13001, 3195, 1, TARGET_SLOT_ORDINAL_BUFF, 4999}));
	EXPECT_EQ(state.effects[2], (AbnormalEntry{0x00A13001, 8291, 2, TARGET_SLOT_ORDINAL_SPEC2, -1}))
		<< "the level byte comes before the slot ordinal (:35-36), and the remaining time is signed";
}

TEST(SkillDecodersTest, AbnormalStateRejectsWhatJavaCannotWrite) {
	const std::vector<uint8_t> empty = PacketWriter().D(0).D(0).D(0).C(0).H(0).data;
	EXPECT_TRUE(decodeAbnormalState(empty).effects.empty()) << "InstanceBuff and the invisibility commands send an empty list with slot 0";
	for (const size_t filler : {4u, 8u}) {
		std::vector<uint8_t> nonZero = empty;
		nonZero[filler] = 1;
		EXPECT_THROW(decodeAbnormalState(nonZero), DecodeError) << "the zero int at offset " << filler;
	}
	const std::vector<uint8_t> badSlot = PacketWriter().D(0).D(0).D(0).C(1).H(1).D(1).H(1).C(1).C(8).D(0).data;
	EXPECT_THROW(decodeAbnormalState(badSlot), DecodeError) << "SkillTargetSlot has eight constants, so ordinal 8 cannot be written";
	std::vector<uint8_t> countTooHigh = empty;
	countTooHigh[13] = 1;
	EXPECT_THROW(decodeAbnormalState(countTooHigh), DecodeError) << "an entry the body does not hold";
}

// ---- SM_ABNORMAL_EFFECT -----------------------------------------------------------------------------------------------------------------

TEST(SkillDecodersTest, AbnormalEffectOfRootOnAnNpcFromAHandBuiltBody) {
	// SM_ABNORMAL_EFFECT.java:40-55: the Mage's 1328 Root on npc 210663 as the broadcast of EffectController.broadCastEffects (X6)
	const std::vector<uint8_t> body = {
		0x02, 0x30, 0xA1, 0x00, // 0: writeD(effected.getObjectId()) = 0x00A13002    (:40)
		0x01,                   // 4: writeC(effectType) = 1, not a Player            (:41)
		0x00, 0x00, 0x00, 0x00, // 5: writeD(0), "TODO time"                          (:42)
		0x00, 0x40, 0x00, 0x00, // 9: writeD(abnormals) = 0x4000                       (:43)
		0x00, 0x00, 0x00, 0x00, // 13: writeD(0)                                       (:44)
		0x02,                   // 17: writeC(slots) = DEBUFF's id                     (:45)
		0x01, 0x00,             // 18: writeH(filtered.size()) = 1                     (:46)
		0x30, 0x05,             // 20: writeH(skillId) = 1328                          (:52)
		0x01,                   // 22: writeC(skillLevel) = 1                          (:53)
		0x01,                   // 23: writeC(targetSlot.ordinal()) = DEBUFF           (:54)
		0x20, 0x4E, 0x00, 0x00, // 24: writeD(remainingTimeToDisplay) = 20000          (:55)
	};
	ASSERT_EQ(body.size(), 28u);
	const AbnormalEffect effect = decodeAbnormalEffect(body);
	EXPECT_EQ(effect.effectedObjectId, 0x00A13002);
	EXPECT_EQ(effect.effectType, ABNORMAL_EFFECT_TYPE_CREATURE);
	EXPECT_EQ(effect.abnormals, 0x4000) << "the abnormals at offset 9, after the zero time int";
	EXPECT_EQ(effect.slots, TARGET_SLOT_ID_DEBUFF) << "the slot MASK at offset 17";
	ASSERT_EQ(effect.effects.size(), 1u);
	EXPECT_EQ(effect.effects[0], (AbnormalEntry{std::nullopt, 1328, 1, TARGET_SLOT_ORDINAL_DEBUFF, 20000}))
		<< "no effector id for a creature that is not a Player (:49-51)";

	// the same bytes with an effect type the constructor cannot set: they would still decode as the type 1 layout, so only the check fails them
	for (const uint8_t type : std::initializer_list<uint8_t>{0, 3}) {
		std::vector<uint8_t> otherType = body;
		otherType[4] = type;
		EXPECT_THROW(decodeAbnormalEffect(otherType), DecodeError) << "effect type " << int{type} << ": the constructor writes only 1 or 2 (:34)";
	}
}

TEST(SkillDecodersTest, AbnormalEffectOfAnotherPlayerCarriesTheEffectorIds) {
	AbnormalEffect player;
	player.effectedObjectId = 0x00A13005;
	player.effectType = ABNORMAL_EFFECT_TYPE_PLAYER;
	player.abnormals = 0;
	player.slots = TARGET_SLOT_FULLSLOTS;
	player.effects = {AbnormalEntry{0x00A13005, 3195, 1, TARGET_SLOT_ORDINAL_BUFF, 5000}, AbnormalEntry{0x00A13007, 1328, 2, TARGET_SLOT_ORDINAL_DEBUFF, 19000}};
	PacketWriter writer;
	abnormalEffectBody(writer, player);
	EXPECT_EQ(writer.data.size(), 20u + 2u * 12u) << "type 2 falls through from the effector id into the type 1 fields (:49-56): 4 + 8 bytes";
	const AbnormalEffect decoded = decodeAbnormalEffect(writer.data);
	EXPECT_EQ(decoded.effectType, ABNORMAL_EFFECT_TYPE_PLAYER);
	EXPECT_EQ(decoded.slots, TARGET_SLOT_FULLSLOTS);
	EXPECT_EQ(decoded.effects, player.effects);

	std::vector<uint8_t> otherType = writer.data;
	otherType[4] = 3;
	EXPECT_THROW(decodeAbnormalEffect(otherType), DecodeError) << "the constructor writes only 1 or 2 (:34)";
	std::vector<uint8_t> time = writer.data;
	time[5] = 1;
	EXPECT_THROW(decodeAbnormalEffect(time), DecodeError) << "the time int is a literal 0 (:42)";
	std::vector<uint8_t> trailingZero = writer.data;
	trailingZero[13] = 1;
	EXPECT_THROW(decodeAbnormalEffect(trailingZero), DecodeError) << "the int after the abnormals is a literal 0 (:44)";
	std::vector<uint8_t> asCreature = writer.data;
	asCreature[4] = ABNORMAL_EFFECT_TYPE_CREATURE;
	EXPECT_THROW(decodeAbnormalEffect(asCreature), DecodeError) << "read as type 1 the effector ids shift every later field";
}

// ---- SM_SKILL_COOLDOWN ------------------------------------------------------------------------------------------------------------------

TEST(SkillDecodersTest, SkillCooldownFromAHandBuiltBody) {
	// SM_SKILL_COOLDOWN.java:46-52: the enter-world list with two cooldowns
	const std::vector<uint8_t> body = {
		0x02, 0x00,             // 0: writeH(cooldowns.size()) = 2              (:46)
		0x00,                   // 2: writeC(notify ? 1 : 0) = 0                 (:47)
		0x30, 0x0B,             // 3: writeH(skillId) = 2864                     (:49)
		0x07, 0x00, 0x00, 0x00, // 5: writeD(remainingSeconds) = 7               (:50)
		0x10, 0x27, 0x00, 0x00, // 9: writeD(durationMillis) = 10000             (:51)
		0x30, 0x05,             // 13: 1328
		0x3B, 0x00, 0x00, 0x00, // 15: 59
		0x60, 0xEA, 0x00, 0x00, // 19: 60000
	};
	const SkillCooldown cooldown = decodeSkillCooldown(body);
	EXPECT_FALSE(cooldown.notify);
	ASSERT_EQ(cooldown.cooldowns.size(), 2u);
	EXPECT_EQ(cooldown.cooldowns[0], (SkillCooldownEntry{2864, 7, 10000}));
	EXPECT_EQ(cooldown.cooldowns[1], (SkillCooldownEntry{1328, 59, 60000})) << "the remaining SECONDS come before the duration in ms";

	std::vector<uint8_t> flag = body;
	flag[2] = 2;
	EXPECT_THROW(decodeSkillCooldown(flag), DecodeError) << "notify is a boolean";
	EXPECT_TRUE(decodeSkillCooldown(PacketWriter().H(0).C(1).data).notify) << "an empty reset list still notifies";
	EXPECT_THROW(decodeSkillCooldown(std::span<const uint8_t>(body).first(body.size() - 1)), DecodeError);
}

// ---- SM_STATUPDATE_MP -------------------------------------------------------------------------------------------------------------------

TEST(SkillDecodersTest, StatUpdateMpFromAHandBuiltBody) {
	// SM_STATUPDATE_MP.java:27-28: X4's MP after Flame Bolt, 405 - 19 = 386 of 405
	const std::vector<uint8_t> body = {
		0x82, 0x01, 0x00, 0x00, // 0: writeD(currentMp) = 386 (:27)
		0x95, 0x01, 0x00, 0x00, // 4: writeD(maxMp) = 405     (:28)
	};
	const StatUpdateMp mp = decodeStatUpdateMp(body);
	EXPECT_EQ(mp.currentMp, 386) << "the CURRENT value comes first";
	EXPECT_EQ(mp.maxMp, 405);
	EXPECT_THROW(decodeStatUpdateMp(std::span<const uint8_t>(body).first(7)), DecodeError);
	EXPECT_EQ(ATTACK_STATUS_TYPE_USED_MP, 23) << "SM_ATTACK_STATUS.TYPE.USED_MP, the type of X4's MP cost";
}

} // namespace
} // namespace aion::gameserver::scenario::decoders
