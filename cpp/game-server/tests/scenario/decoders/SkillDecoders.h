#pragma once

// The seven skill and effect packets the M5b-2 gate reads (m5b2-plan.md §5 item G-02, §10.3 assertions X2-X13): SM_CASTSPELL,
// SM_CASTSPELL_RESULT, SM_SKILL_CANCEL, SM_ABNORMAL_STATE, SM_ABNORMAL_EFFECT, SM_SKILL_COOLDOWN and SM_STATUPDATE_MP.
//
// **m5a-plan.md D9, and the only reason this file is evidence at all:** every layout below is written from the Java `writeImpl` under
// game-server/src/com/aionemu/gameserver/network/aion/serverpackets/ and from the model enums those methods call (SkillTargetSlot, SpellStatus,
// EffectResult, EffectReserved.ResourceType under skillengine/model/, and SM_ATTACK_STATUS.TYPE for the MP cost). Nothing here includes, calls
// or mirrors a C++ serverpackets header - in particular not the C++ SM_CASTSPELL, SM_CASTSPELL_RESULT or SM_ABNORMAL_* classes, which exist
// and are ported (m5b2-plan.md §1). FakeGameClient decrypts and frames with the server's own protocol code, so a symmetric width or order error
// in the port would stay invisible unless the expectation was written from Java alone.
//
// Every decode function consumes the body exactly and throws DecodeError otherwise. Java constants that carry no data are verified rather than
// skipped (SM_CASTSPELL's `writeC(0x00)`, the eight zero words of the target type 2 arms, SM_CASTSPELL_RESULT's `writeC(0)` / `writeH(0)`,
// SM_ABNORMAL_STATE's and SM_ABNORMAL_EFFECT's zero ints), and so are the closed value sets a writeImpl can produce (the chain status byte,
// the item arm marker, the EffectResult, SpellStatus and ResourceType ids, the SkillTargetSlot ordinals, the 0/1 flags).
//
// Three things the M5b-2 plan's §2.7 summary of these packets does not say, read off the Java and worth knowing before a gate row is written:
//   * SM_CASTSPELL does not end with the cast duration and a zero byte: `writeH(castDuration)` and `writeC(0x00)` are followed by
//     `writeF(castSpeed)` and a 0/1 byte (SM_CASTSPELL.java:75-78).
//   * An effect on an NPC is never an SM_ABNORMAL_STATE. That packet goes only to the effected player itself, from PlayerEffectController
//     (updatePlayerEffectIcons, addSavedEffect: PlayerEffectController.java:88, :119); the effects of any other creature reach a client as the
//     SM_ABNORMAL_EFFECT that EffectController.broadCastEffects broadcasts to the players who know it (EffectController.java:304-308), with
//     effectType 1 and no effector id per entry. X6 (Root on the monster) therefore reads SM_ABNORMAL_EFFECT; X7, X10 and X11 (effects on the
//     character) read SM_ABNORMAL_STATE. broadcastPacket(object, packet) skips the object itself (PacketSendUtility.java:98-100), so the
//     character never receives its own SM_ABNORMAL_EFFECT.
//   * The per-effect slot both abnormal packets carry is SkillTargetSlot.ordinal() (BUFF 0 ... NONE 7), while the packet-level slot byte is
//     SkillTargetSlot.getId(), a mask (BUFF 1 ... NONE 128, FULLSLOTS 127).

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "decoders/PacketDecoders.h" // BodyReader and DecodeError

namespace aion::gameserver::scenario::decoders {

/** a position the skill packets write as three writeF */
struct SkillPosition {
	float x = 0, y = 0, z = 0;

	bool operator==(const SkillPosition&) const = default;
};

// ---- the target type arms of SM_CASTSPELL and SM_CASTSPELL_RESULT ------------------------------------------------------------------------

/**
 * The target type both packets switch on (SM_CASTSPELL.java:51-74, SM_CASTSPELL_RESULT.java:54-78; CM_CASTSPELL.java:21: "0 - obj id,
 * 1 - point location, 2 - unk, 3 - object not in sight(skill 1606)? 4 - unk"). 0, 3 and 4 write an object id, 1 a point, 2 a point and eight
 * zero words; any other value writes nothing at all (the switches have no default arm).
 */
constexpr uint8_t CAST_TARGET_OBJECT = 0;
constexpr uint8_t CAST_TARGET_POINT = 1;
constexpr uint8_t CAST_TARGET_POINT_EXTENDED = 2;
constexpr uint8_t CAST_TARGET_OBJECT_NOT_IN_SIGHT = 3;
constexpr uint8_t CAST_TARGET_OBJECT_4 = 4;

// ---- SM_CASTSPELL -----------------------------------------------------------------------------------------------------------------------

/** SM_CASTSPELL (SM_CASTSPELL.java:46-79): the cast bar, sent by Skill.startCast to the effector and every player that knows it */
struct CastSpell {
	int32_t effectorObjectId = 0;
	uint16_t spellId = 0;
	/** the skill level of the cast (Skill.skillLevel), truncated to a byte by writeC */
	uint8_t level = 0;
	uint8_t targetType = 0;
	/** target types 0, 3 and 4 */
	int32_t targetObjectId = 0;
	/** target types 1 and 2 */
	std::optional<SkillPosition> targetPosition;
	/** the cast bar in ms (Skill.castDuration), a **short**: SM_CASTSPELL.java:75 truncates anything above 65,535 */
	uint16_t castDuration = 0;
	/** Skill.castSpeedForAnimationBoostAndChargeSkills (:77) */
	float castSpeed = 0;
	/** SkillTemplate.isApplyCastingTimeBonus, through Skill.allowAnimationBoostByCastSpeed (:78) */
	bool allowAnimationBoostByCastSpeed = false;
};

CastSpell decodeCastSpell(std::span<const uint8_t> body);

// ---- SM_CASTSPELL_RESULT ----------------------------------------------------------------------------------------------------------------

/** the status byte of SM_CASTSPELL_RESULT.java:89-94: 16 exactly when the effect list is empty, else 32 on a chain success, else 0 */
constexpr uint8_t CAST_RESULT_NO_CHAIN = 0;
constexpr uint8_t CAST_RESULT_NO_EFFECT = 16;
constexpr uint8_t CAST_RESULT_CHAIN_SUCCESS = 32;

/** the byte that opens the item arm (SM_CASTSPELL_RESULT.java:97) and the two the other arm writes (:102-105) */
constexpr uint8_t CAST_RESULT_ITEM_ARM = 2;
constexpr uint8_t CAST_RESULT_SKILL_ARM = 0;
constexpr uint8_t CAST_RESULT_PENALTY_SKILL_ARM = 4;

/** EffectResult.getId() (skillengine/model/EffectResult.java:8-14), the byte SM_CASTSPELL_RESULT.java:127 writes per effect */
constexpr uint8_t EFFECT_RESULT_NORMAL = 0;
constexpr uint8_t EFFECT_RESULT_ABSORBED = 1;
constexpr uint8_t EFFECT_RESULT_CONFLICT = 2;
constexpr uint8_t EFFECT_RESULT_DODGE = 3;
constexpr uint8_t EFFECT_RESULT_RESIST = 4;
constexpr uint8_t EFFECT_RESULT_IMMUNE = 5;
constexpr uint8_t EFFECT_RESULT_CANCELED_DUE_TO_TOO_MANY_EFFECTS = 6;

/**
 * SpellStatus.getId() as SM_CASTSPELL_RESULT.java:140 writes it: a writeC of an int, so RESIST (256) reaches the wire as 0 - the same byte as
 * NONE - and DODGE (128) and DODGE2 (-128) both as 0x80 (skillengine/model/SpellStatus.java:8-18). The switch of :144-167 runs on the INT,
 * which is why the byte still decides the arm: 1, 2, 4 and 8 write the target position, 16 the effector heading, and 0, 32, 64 and 0x80 take
 * the default arm.
 */
constexpr uint8_t SPELL_STATUS_NONE_OR_RESIST = 0;
constexpr uint8_t SPELL_STATUS_STUMBLE = 1;
constexpr uint8_t SPELL_STATUS_STAGGER = 2;
constexpr uint8_t SPELL_STATUS_OPENAERIAL = 4;
constexpr uint8_t SPELL_STATUS_CLOSEAERIAL = 8;
constexpr uint8_t SPELL_STATUS_SPIN = 16;
constexpr uint8_t SPELL_STATUS_BLOCK = 32;
constexpr uint8_t SPELL_STATUS_PARRY = 64;
constexpr uint8_t SPELL_STATUS_DODGE = 0x80;

/** EffectReserved.ResourceType.getValue() (skillengine/model/EffectReserved.java:17-22), the byte SM_CASTSPELL_RESULT.java:172 writes */
constexpr uint8_t RESERVED_RESOURCE_HP = 0;
constexpr uint8_t RESERVED_RESOURCE_MP = 1;
constexpr uint8_t RESERVED_RESOURCE_FP = 2;
constexpr uint8_t RESERVED_RESOURCE_DP = 3;

/** one EffectReserved of an effect (SM_CASTSPELL_RESULT.java:171-209) */
struct CastResultReserved {
	uint8_t resourceType = 0;
	/** EffectReserved.getValueToSend() */
	int32_t value = 0;
	/** AttackStatus.getId(), signed (the ATTACK_STATUS_* constants of CombatDecoders.h) */
	int8_t attackStatusId = 0;
	/** Effect.getShieldDefense(); 0 and 2 write nothing more, 8 and 10 three ints, every other value seven */
	uint8_t shieldDefense = 0;
	int32_t protectorId = 0, protectedDamage = 0, protectedSkillId = 0;
	int32_t reflectedDamage = 0, reflectedSkillId = 0, mpAbsorbed = 0, mpShieldSkillId = 0;

	bool operator==(const CastResultReserved&) const = default;
};

/** one entry of the effect list (SM_CASTSPELL_RESULT.java:122-210) */
struct CastResultEffect {
	/** the effected creature, or the effector itself for a point skill whose effect has no original effected (:129-133) */
	int32_t effectedObjectId = 0;
	uint8_t effectResult = 0;
	/** Effect.getEffectedHp(), or the target's HP percentage when that is -1 (:128) */
	uint8_t targetHpPercentage = 0;
	uint8_t effectorHpPercentage = 0;
	/** the SPELL_STATUS_* byte */
	uint8_t spellStatus = 0;
	/** Effect.getSuccessfulEffectsAsByte() */
	uint8_t successfulEffects = 0;
	/** Effect.getCarvedSignet() */
	uint8_t carvedSignet = 0;
	/** spell status 1, 2, 4 and 8, or the default arm with a PULL / PULL_NPC / SIMPLE_MOVE_BACK sub effect (:145-165) */
	std::optional<SkillPosition> targetPosition;
	/** spell status 16 (SPIN): the effector's heading (:154) */
	std::optional<uint8_t> effectorHeading;
	std::vector<CastResultReserved> reserved;

	bool operator==(const CastResultEffect&) const = default;
};

/**
 * The one branch of SM_CASTSPELL_RESULT the bytes cannot name: in the default arm of the spell status switch (:156-166) the target position
 * follows only when Effect.getSubEffectType() is PULL, PULL_NPC or SIMPLE_MOVE_BACK, which the packet does not carry. The default is that no
 * effect has such a sub effect - none of the skills of the M5b-2 gate does (m5b2-plan.md §2.4) - and a caller that expects one names the
 * index of the effect in the list.
 */
struct CastSpellResultOptions {
	std::vector<size_t> effectsWithMoveSubEffect;
};

/** SM_CASTSPELL_RESULT (SM_CASTSPELL_RESULT.java:50-211): the end of a cast, sent by Skill.sendCastSpellEnd */
struct CastSpellResult {
	int32_t effectorObjectId = 0;
	uint8_t targetType = 0;
	/** target types 0, 3 and 4: Skill.getFirstTarget().getObjectId() */
	int32_t targetObjectId = 0;
	/** target types 1 and 2: Skill.getX/Y/Z() */
	std::optional<SkillPosition> targetPosition;
	uint16_t skillId = 0;
	/** SkillTemplate.getLvl(), the TEMPLATE's level - not the level of the cast, which SM_CASTSPELL carries */
	uint8_t skillTemplateLevel = 0;
	/** Skill.getCooldown(), in units of 100 ms */
	int32_t cooldown = 0;
	/** the hit time of the cast, truncated to a short */
	uint16_t hitTime = 0;
	/** CAST_RESULT_NO_CHAIN, CAST_RESULT_NO_EFFECT or CAST_RESULT_CHAIN_SUCCESS */
	uint8_t chainStatus = 0;
	/** the item arm (:96-100): a combat activated item */
	bool itemArm = false;
	int32_t itemObjectId = 0, itemTemplateId = 0;
	/** the other arm (:101-119): 4 for a PENALTY skill method, 0 otherwise */
	bool penaltySkill = false;
	uint8_t dashStatus = 0;
	/** dash status 1, 2, 3, 4 and 6: Skill.getH() and the skill position (:108-117) */
	std::optional<uint8_t> dashHeading;
	std::optional<SkillPosition> dashPosition;
	std::vector<CastResultEffect> effects;
};

CastSpellResult decodeCastSpellResult(std::span<const uint8_t> body, const CastSpellResultOptions& options = {});

// ---- SM_SKILL_CANCEL --------------------------------------------------------------------------------------------------------------------

/** SM_SKILL_CANCEL (SM_SKILL_CANCEL.java:20-24), a 6-byte body: sent by PlayerController.cancelCurrentSkill and CreatureController */
struct SkillCancel {
	int32_t creatureObjectId = 0;
	uint16_t skillId = 0;
};

SkillCancel decodeSkillCancel(std::span<const uint8_t> body);

// ---- SM_ABNORMAL_STATE and SM_ABNORMAL_EFFECT -------------------------------------------------------------------------------------------

/** SkillTargetSlot.ordinal() (skillengine/model/SkillTargetSlot.java:12-19), the byte both abnormal packets write per effect */
constexpr uint8_t TARGET_SLOT_ORDINAL_BUFF = 0;
constexpr uint8_t TARGET_SLOT_ORDINAL_DEBUFF = 1;
constexpr uint8_t TARGET_SLOT_ORDINAL_CHANT = 2;
constexpr uint8_t TARGET_SLOT_ORDINAL_SPEC = 3;
constexpr uint8_t TARGET_SLOT_ORDINAL_SPEC2 = 4;
constexpr uint8_t TARGET_SLOT_ORDINAL_BOOST = 5;
constexpr uint8_t TARGET_SLOT_ORDINAL_NOSHOW = 6;
constexpr uint8_t TARGET_SLOT_ORDINAL_NONE = 7;

/** SkillTargetSlot.getId() and SkillTargetSlot.FULLSLOTS (:12-23), the slot mask byte of both abnormal packets */
constexpr uint8_t TARGET_SLOT_ID_BUFF = 1;
constexpr uint8_t TARGET_SLOT_ID_DEBUFF = 2;
constexpr uint8_t TARGET_SLOT_ID_SPEC2 = 16;
constexpr uint8_t TARGET_SLOT_FULLSLOTS = 127;

/** one effect of SM_ABNORMAL_STATE (SM_ABNORMAL_STATE.java:32-38) or of SM_ABNORMAL_EFFECT (SM_ABNORMAL_EFFECT.java:47-61) */
struct AbnormalEntry {
	/** Effect.getEffectorId(); SM_ABNORMAL_EFFECT writes it only for a Player effected (effectType 2) */
	std::optional<int32_t> effectorObjectId;
	uint16_t skillId = 0;
	/** Effect.getSkillLevel() truncated to a byte (the soul sickness carries the death count here) */
	uint8_t skillLevel = 0;
	/** the TARGET_SLOT_ORDINAL_* byte */
	uint8_t targetSlotOrdinal = 0;
	/** Effect.getRemainingTimeToDisplay(): -1 for a permanent effect, else the milliseconds left when the packet was written */
	int32_t remainingTimeToDisplay = 0;

	bool operator==(const AbnormalEntry&) const = default;
};

/** SM_ABNORMAL_STATE (SM_ABNORMAL_STATE.java:24-39): the effect icons of the receiving player itself */
struct AbnormalState {
	/** EffectController.getAbnormals(), the abnormal state bit mask */
	int32_t abnormals = 0;
	/** the slot mask of the update: a TARGET_SLOT_ID_*, TARGET_SLOT_FULLSLOTS, or 0 (InstanceBuff, the invisibility commands) */
	uint8_t slot = 0;
	/** every entry has an effector id */
	std::vector<AbnormalEntry> effects;
};

AbnormalState decodeAbnormalState(std::span<const uint8_t> body);

/** the effectType byte of SM_ABNORMAL_EFFECT.java:34: 2 for a Player effected, 1 for every other creature */
constexpr uint8_t ABNORMAL_EFFECT_TYPE_CREATURE = 1;
constexpr uint8_t ABNORMAL_EFFECT_TYPE_PLAYER = 2;

/** SM_ABNORMAL_EFFECT (SM_ABNORMAL_EFFECT.java:37-62): the effects of another creature, broadcast by EffectController.broadCastEffects */
struct AbnormalEffect {
	int32_t effectedObjectId = 0;
	uint8_t effectType = 0;
	int32_t abnormals = 0;
	/** the slot mask the effects were filtered with (TARGET_SLOT_FULLSLOTS for the whole list) */
	uint8_t slots = 0;
	/** effectorObjectId is set exactly for effectType 2 */
	std::vector<AbnormalEntry> effects;
};

AbnormalEffect decodeAbnormalEffect(std::span<const uint8_t> body);

// ---- SM_SKILL_COOLDOWN ------------------------------------------------------------------------------------------------------------------

/** one cooldown of SM_SKILL_COOLDOWN (SM_SKILL_COOLDOWN.java:48-52) */
struct SkillCooldownEntry {
	uint16_t skillId = 0;
	/** (expirationTimeMillis - now) / 1000, at least 0, and 0 for an expiration of 0 */
	int32_t remainingSeconds = 0;
	/** SkillTemplate.getCooldown() * 100 of THIS skill id, without the cooldown_delta_lv term */
	int32_t durationMillis = 0;

	bool operator==(const SkillCooldownEntry&) const = default;
};

/** SM_SKILL_COOLDOWN (SM_SKILL_COOLDOWN.java:44-53) */
struct SkillCooldown {
	/** 1 triggers the client's notification sound and animation (:47) */
	bool notify = false;
	std::vector<SkillCooldownEntry> cooldowns;
};

SkillCooldown decodeSkillCooldown(std::span<const uint8_t> body);

// ---- SM_STATUPDATE_MP -------------------------------------------------------------------------------------------------------------------

/** SM_STATUPDATE_MP (SM_STATUPDATE_MP.java:25-29), an 8-byte body of two ints: absolute MP, not a percentage */
struct StatUpdateMp {
	int32_t currentMp = 0;
	int32_t maxMp = 0;
};

StatUpdateMp decodeStatUpdateMp(std::span<const uint8_t> body);

// ---- the MP cost on SM_ATTACK_STATUS ----------------------------------------------------------------------------------------------------

/**
 * SM_ATTACK_STATUS.TYPE.USED_MP (SM_ATTACK_STATUS.java:50), the type MpCondition.validate reduces the MP with (MpCondition.java:34). The
 * USED_MP arm of writeImpl writes the value NEGATED (SM_ATTACK_STATUS.java:137-140), so Flame Bolt's 19 MP reach the wire as -19, and the
 * percentage byte is the MP percentage. decodeAttackStatus of CombatDecoders.h reads the packet; this is the one type constant it lacks.
 */
constexpr uint8_t ATTACK_STATUS_TYPE_USED_MP = 23;

} // namespace aion::gameserver::scenario::decoders
