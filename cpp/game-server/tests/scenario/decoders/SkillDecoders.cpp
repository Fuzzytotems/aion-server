#include "decoders/SkillDecoders.h"

#include <algorithm>
#include <string>

namespace aion::gameserver::scenario::decoders {

namespace {

/** reads a byte and fails unless it is 0 or 1 (a Java `flag ? 1 : 0`) */
bool readFlag(BodyReader& reader, std::string_view what) {
	const uint8_t value = reader.C();
	if (value > 1)
		reader.fail(std::string(what) + ": expected 0 or 1, got " + std::to_string(value));
	return value == 1;
}

SkillPosition readPosition(BodyReader& reader) {
	SkillPosition position;
	position.x = reader.F();
	position.y = reader.F();
	position.z = reader.F();
	return position;
}

/**
 * The target type arms of SM_CASTSPELL.java:51-74 and SM_CASTSPELL_RESULT.java:54-78, which are the same shape: 0, 3 and 4 an object id, 1 the
 * three coordinates, 2 the coordinates and eight zero words (writeD(0) in SM_CASTSPELL, writeF(0) in SM_CASTSPELL_RESULT - both four zero bytes),
 * and nothing for any other type, because neither switch has a default arm.
 */
void readTargetArm(BodyReader& reader, uint8_t targetType, int32_t& objectId, std::optional<SkillPosition>& position, std::string_view source) {
	switch (targetType) {
		case CAST_TARGET_OBJECT:
		case CAST_TARGET_OBJECT_NOT_IN_SIGHT:
		case CAST_TARGET_OBJECT_4:
			objectId = reader.D();
			break;
		case CAST_TARGET_POINT:
			position = readPosition(reader);
			break;
		case CAST_TARGET_POINT_EXTENDED:
			position = readPosition(reader);
			reader.expectZeros(32, std::string("the eight zero words of the target type 2 arm (") + std::string(source) + ")");
			break;
		default:
			break;
	}
}

/** SM_CASTSPELL_RESULT.java:188-207: the tail one shieldDefense value selects */
void readReservedShield(BodyReader& reader, CastResultReserved& reserved) {
	switch (reserved.shieldDefense) {
		case 0:
		case 2:
			break; // :190-192
		case 8:
		case 10:
			reserved.protectorId = reader.D();      // :195
			reserved.protectedDamage = reader.D();  // :196
			reserved.protectedSkillId = reader.D(); // :197
			break;
		default:
			reserved.protectorId = reader.D();      // :200
			reserved.protectedDamage = reader.D();  // :201
			reserved.protectedSkillId = reader.D(); // :202
			reserved.reflectedDamage = reader.D();  // :203
			reserved.reflectedSkillId = reader.D(); // :204
			reserved.mpAbsorbed = reader.D();       // :205
			reserved.mpShieldSkillId = reader.D();  // :206
			break;
	}
}

/** the abnormal entry both packets share from the skill id on (SM_ABNORMAL_STATE.java:34-37, SM_ABNORMAL_EFFECT.java:52-55) */
void readAbnormalEntryTail(BodyReader& reader, AbnormalEntry& entry) {
	entry.skillId = reader.H();
	entry.skillLevel = reader.C();
	entry.targetSlotOrdinal = reader.C();
	if (entry.targetSlotOrdinal > TARGET_SLOT_ORDINAL_NONE)
		reader.fail("target slot ordinal " + std::to_string(entry.targetSlotOrdinal) + " is not a SkillTargetSlot ordinal (SkillTargetSlot.java:12-19 "
			"has eight constants)");
	entry.remainingTimeToDisplay = reader.D();
}

} // namespace

// ---- SM_CASTSPELL -----------------------------------------------------------------------------------------------------------------------

CastSpell decodeCastSpell(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_CASTSPELL");
	CastSpell cast;
	cast.effectorObjectId = reader.D(); // SM_CASTSPELL.java:47
	cast.spellId = reader.H();          // :48
	cast.level = reader.C();            // :49
	cast.targetType = reader.C();       // :50
	readTargetArm(reader, cast.targetType, cast.targetObjectId, cast.targetPosition, "SM_CASTSPELL.java:66-73"); // :51-74
	cast.castDuration = reader.H();     // :75
	reader.expectC(0, "the unknown byte after the cast duration (SM_CASTSPELL.java:76)");
	cast.castSpeed = reader.F();        // :77
	cast.allowAnimationBoostByCastSpeed = readFlag(reader, "allowAnimationBoostByCastSpeed (SM_CASTSPELL.java:78)");
	reader.expectFullyConsumed();
	return cast;
}

// ---- SM_CASTSPELL_RESULT ----------------------------------------------------------------------------------------------------------------

CastSpellResult decodeCastSpellResult(std::span<const uint8_t> body, const CastSpellResultOptions& options) {
	BodyReader reader(body, "SM_CASTSPELL_RESULT");
	CastSpellResult result;
	result.effectorObjectId = reader.D(); // SM_CASTSPELL_RESULT.java:52
	result.targetType = reader.C();       // :53
	readTargetArm(reader, result.targetType, result.targetObjectId, result.targetPosition, "SM_CASTSPELL_RESULT.java:69-76"); // :54-78
	result.skillId = reader.H();            // :79
	result.skillTemplateLevel = reader.C(); // :80
	result.cooldown = reader.D();           // :81
	result.hitTime = reader.H();            // :82
	reader.expectC(0, "the unknown byte after the hit time (SM_CASTSPELL_RESULT.java:83)");
	result.chainStatus = reader.C();        // :89-94
	if (result.chainStatus != CAST_RESULT_NO_CHAIN && result.chainStatus != CAST_RESULT_NO_EFFECT && result.chainStatus != CAST_RESULT_CHAIN_SUCCESS)
		reader.fail("the chain status byte is " + std::to_string(result.chainStatus) + ", but SM_CASTSPELL_RESULT.java:89-94 writes only 0, 16 or 32");

	const uint8_t arm = reader.C(); // :97 (the item arm) or :102-105 (the other one)
	switch (arm) {
		case CAST_RESULT_ITEM_ARM:
			result.itemArm = true;
			result.itemObjectId = reader.D();   // :98
			result.itemTemplateId = reader.D(); // :99
			reader.expectC(0, "the byte that closes the item arm (SM_CASTSPELL_RESULT.java:100)");
			break;
		case CAST_RESULT_SKILL_ARM:
		case CAST_RESULT_PENALTY_SKILL_ARM:
			result.penaltySkill = arm == CAST_RESULT_PENALTY_SKILL_ARM;
			result.dashStatus = reader.C(); // :106
			switch (result.dashStatus) {    // :107-118
				case 1:
				case 2:
				case 3:
				case 4:
				case 6:
					result.dashHeading = reader.C();          // :113
					result.dashPosition = readPosition(reader); // :114-116
					break;
				default:
					break;
			}
			break;
		default:
			reader.fail("the byte after the chain status is " + std::to_string(arm) +
				", but SM_CASTSPELL_RESULT.java:96-105 writes 2 (a combat activated item), 4 (a penalty skill) or 0");
	}

	const uint16_t effectCount = reader.H(); // :121
	if ((effectCount == 0) != (result.chainStatus == CAST_RESULT_NO_EFFECT))
		reader.fail("the chain status byte is " + std::to_string(result.chainStatus) + " with " + std::to_string(effectCount) +
			" effects, but SM_CASTSPELL_RESULT.java:89-90 writes 16 exactly when the effect list is empty");
	for (uint16_t i = 0; i < effectCount; i++) {
		CastResultEffect effect;
		effect.effectedObjectId = reader.D(); // :126 or :130
		effect.effectResult = reader.C();     // :127 or :131
		if (effect.effectResult > EFFECT_RESULT_CANCELED_DUE_TO_TOO_MANY_EFFECTS)
			reader.fail("effect result " + std::to_string(effect.effectResult) + " is not an EffectResult id (EffectResult.java:8-14)");
		effect.targetHpPercentage = reader.C();   // :128 or :132
		effect.effectorHpPercentage = reader.C(); // :135
		effect.spellStatus = reader.C();          // :140
		effect.successfulEffects = reader.C();    // :141
		reader.expectH(0, "the zero short after the successful effects (SM_CASTSPELL_RESULT.java:142)");
		effect.carvedSignet = reader.C(); // :143
		switch (effect.spellStatus) {     // :144-167
			case SPELL_STATUS_STUMBLE:
			case SPELL_STATUS_STAGGER:
			case SPELL_STATUS_OPENAERIAL:
			case SPELL_STATUS_CLOSEAERIAL:
				effect.targetPosition = readPosition(reader); // :149-151
				break;
			case SPELL_STATUS_SPIN:
				effect.effectorHeading = reader.C(); // :154
				break;
			case SPELL_STATUS_NONE_OR_RESIST:
			case SPELL_STATUS_BLOCK:
			case SPELL_STATUS_PARRY:
			case SPELL_STATUS_DODGE:
				// :156-165, the position only for a PULL, PULL_NPC or SIMPLE_MOVE_BACK sub effect, which the packet does not carry
				if (std::ranges::find(options.effectsWithMoveSubEffect, static_cast<size_t>(i)) != options.effectsWithMoveSubEffect.end())
					effect.targetPosition = readPosition(reader);
				break;
			default:
				reader.fail("spell status byte " + std::to_string(effect.spellStatus) +
					" is none of the ids of SpellStatus.java:8-18 as writeC truncates them (0, 1, 2, 4, 8, 16, 32, 64, 0x80)");
		}
		const uint8_t reservedCount = reader.C(); // :170
		for (uint8_t r = 0; r < reservedCount; r++) {
			CastResultReserved reserved;
			reserved.resourceType = reader.C(); // :172
			if (reserved.resourceType > RESERVED_RESOURCE_DP)
				reader.fail("resource type " + std::to_string(reserved.resourceType) + " is not an EffectReserved.ResourceType value (EffectReserved.java:17-22)");
			reserved.value = reader.D();           // :173
			reserved.attackStatusId = reader.Cs(); // :174
			reserved.shieldDefense = reader.C();   // :188
			readReservedShield(reader, reserved);  // :189-207
			effect.reserved.push_back(reserved);
		}
		result.effects.push_back(std::move(effect));
	}
	reader.expectFullyConsumed();
	return result;
}

// ---- SM_SKILL_CANCEL --------------------------------------------------------------------------------------------------------------------

SkillCancel decodeSkillCancel(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_SKILL_CANCEL");
	SkillCancel cancel;
	cancel.creatureObjectId = reader.D(); // SM_SKILL_CANCEL.java:22
	cancel.skillId = reader.H();          // :23
	reader.expectFullyConsumed();
	return cancel;
}

// ---- SM_ABNORMAL_STATE ------------------------------------------------------------------------------------------------------------------

AbnormalState decodeAbnormalState(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_ABNORMAL_STATE");
	AbnormalState state;
	state.abnormals = reader.D(); // SM_ABNORMAL_STATE.java:26
	reader.expectD(0, "the first zero int (SM_ABNORMAL_STATE.java:27)");
	reader.expectD(0, "the second zero int (SM_ABNORMAL_STATE.java:28, \"4.5\")");
	state.slot = reader.C();                  // :29
	const uint16_t count = reader.H();        // :30
	for (uint16_t i = 0; i < count; i++) {    // :32-38
		AbnormalEntry entry;
		entry.effectorObjectId = reader.D(); // :33
		readAbnormalEntryTail(reader, entry); // :34-37
		state.effects.push_back(entry);
	}
	reader.expectFullyConsumed();
	return state;
}

// ---- SM_ABNORMAL_EFFECT -----------------------------------------------------------------------------------------------------------------

AbnormalEffect decodeAbnormalEffect(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_ABNORMAL_EFFECT");
	AbnormalEffect decoded;
	decoded.effectedObjectId = reader.D(); // SM_ABNORMAL_EFFECT.java:40
	decoded.effectType = reader.C();       // :41
	// the constructor sets 2 for a Player effected and 1 otherwise (:34), so the `default:` arm of :57-59 is unreachable from Java
	if (decoded.effectType != ABNORMAL_EFFECT_TYPE_CREATURE && decoded.effectType != ABNORMAL_EFFECT_TYPE_PLAYER)
		reader.fail("effect type " + std::to_string(decoded.effectType) + ", but the constructor (SM_ABNORMAL_EFFECT.java:34) sets only 1 or 2");
	reader.expectD(0, "the zero int of the time (SM_ABNORMAL_EFFECT.java:42, \"TODO time\")");
	decoded.abnormals = reader.D(); // :43
	reader.expectD(0, "the zero int after the abnormals (SM_ABNORMAL_EFFECT.java:44)");
	decoded.slots = reader.C();           // :45
	const uint16_t count = reader.H();    // :46
	for (uint16_t i = 0; i < count; i++) { // :47-61
		AbnormalEntry entry;
		if (decoded.effectType == ABNORMAL_EFFECT_TYPE_PLAYER)
			entry.effectorObjectId = reader.D(); // :50, falling through into the case 1 arm
		readAbnormalEntryTail(reader, entry);    // :52-55
		decoded.effects.push_back(entry);
	}
	reader.expectFullyConsumed();
	return decoded;
}

// ---- SM_SKILL_COOLDOWN ------------------------------------------------------------------------------------------------------------------

SkillCooldown decodeSkillCooldown(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_SKILL_COOLDOWN");
	SkillCooldown decoded;
	const uint16_t count = reader.H();                                     // SM_SKILL_COOLDOWN.java:46
	decoded.notify = readFlag(reader, "notify (SM_SKILL_COOLDOWN.java:47)"); // :47
	for (uint16_t i = 0; i < count; i++) {                                 // :48-52
		SkillCooldownEntry entry;
		entry.skillId = reader.H();          // :49
		entry.remainingSeconds = reader.D(); // :50
		entry.durationMillis = reader.D();   // :51
		decoded.cooldowns.push_back(entry);
	}
	reader.expectFullyConsumed();
	return decoded;
}

// ---- SM_STATUPDATE_MP -------------------------------------------------------------------------------------------------------------------

StatUpdateMp decodeStatUpdateMp(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_STATUPDATE_MP");
	StatUpdateMp stat;
	stat.currentMp = reader.D(); // SM_STATUPDATE_MP.java:27
	stat.maxMp = reader.D();     // :28
	reader.expectFullyConsumed();
	return stat;
}

} // namespace aion::gameserver::scenario::decoders
