#pragma once

// The seven combat packets the M5b-1 gate compares the server against (m5b-plan.md §4 item G-04, §6.3 assertions A1a-A8, D1a, R3, P1):
// SM_ATTACK, SM_ATTACK_STATUS, SM_ATTACK_RESPONSE, SM_EMOTION, SM_DIE, SM_LOOT_STATUS and SM_STATUPDATE_HP.
//
// **m5a-plan.md D9, and the only reason this file is evidence at all:** every layout below is written from the Java `writeImpl` under
// game-server/src/com/aionemu/gameserver/network/aion/serverpackets/ and from the model classes those methods call
// (controllers/attack/AttackStatus.java, model/EmotionType.java, model/animations/Attack{Type,Hand}Animation.java, and the TYPE / LOG enums
// declared inside SM_ATTACK_STATUS.java itself). Nothing here includes, calls or mirrors a C++ serverpackets header. FakeGameClient decrypts
// and frames with the server's own protocol code, so a symmetric width or order error in the port would stay invisible unless the expectation
// was written from Java alone; a decoder derived from the C++ packet would agree with any port, right or wrong.
//
// Every decode function consumes the body exactly and throws DecodeError otherwise (D9: "Each decoder must consume the body exactly") - a
// decoder that stops early hides every field after it. Java constants that carry no data are verified rather than skipped: the `writeH(0)` of
// SM_ATTACK.java:92, the zero fillers of its shield arms and its trailing `writeC(0)`, SM_EMOTION's literal `writeD(0)` / `writeC(1)` /
// `writeC(0)` and its three `writeF` constants, and the closed value sets of SM_ATTACK_RESPONSE, SM_LOOT_STATUS and SM_DIE. A port that
// changes one of them fails the decode instead of silently shifting every later field.

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "decoders/PacketDecoders.h" // BodyReader and DecodeError; also the four EmoteManager emotion ids of the M5a async set

namespace aion::gameserver::scenario::decoders {

// ---- SM_ATTACK --------------------------------------------------------------------------------------------------------------------------

/** model/animations/AttackTypeAnimation.java:5-6, the byte SM_ATTACK.java:48 writes */
constexpr uint8_t ATTACK_TYPE_ANIMATION_MELEE = 0;
constexpr uint8_t ATTACK_TYPE_ANIMATION_RANGED = 1;

/** model/animations/AttackHandAnimation.java:5-8, the byte SM_ATTACK.java:49 writes */
constexpr uint8_t ATTACK_HAND_ANIMATION_MAIN_HAND = 0;
constexpr uint8_t ATTACK_HAND_ANIMATION_OFF_HAND = 1;
constexpr uint8_t ATTACK_HAND_ANIMATION_RANDOM = 2;

/**
 * AttackStatus.getId() (AttackStatus.java:7-28), the signed byte SM_ATTACK.java:108 writes per result. The negative ids are the critical
 * variants; `writeC` truncates the Java int to a byte, so -54 reaches the wire as 0xCA and must be read back signed.
 */
constexpr int8_t ATTACK_STATUS_DODGE = 0;
constexpr int8_t ATTACK_STATUS_OFFHAND_DODGE = 1;
constexpr int8_t ATTACK_STATUS_PARRY = 2;
constexpr int8_t ATTACK_STATUS_OFFHAND_PARRY = 3;
constexpr int8_t ATTACK_STATUS_BLOCK = 4;
constexpr int8_t ATTACK_STATUS_OFFHAND_BLOCK = 5;
constexpr int8_t ATTACK_STATUS_RESIST = 6;
constexpr int8_t ATTACK_STATUS_OFFHAND_RESIST = 7;
constexpr int8_t ATTACK_STATUS_BUF = 8;
constexpr int8_t ATTACK_STATUS_OFFHAND_BUF = 9;
constexpr int8_t ATTACK_STATUS_NORMALHIT = 10;
constexpr int8_t ATTACK_STATUS_OFFHAND_NORMALHIT = 11;
constexpr int8_t ATTACK_STATUS_CRITICAL_DODGE = -64;
constexpr int8_t ATTACK_STATUS_CRITICAL_PARRY = -62;
constexpr int8_t ATTACK_STATUS_CRITICAL_BLOCK = -60;
constexpr int8_t ATTACK_STATUS_CRITICAL_RESIST = -58;
constexpr int8_t ATTACK_STATUS_CRITICAL = -54;
constexpr int8_t ATTACK_STATUS_OFFHAND_CRITICAL_DODGE = -47;
constexpr int8_t ATTACK_STATUS_OFFHAND_CRITICAL_PARRY = -45;
constexpr int8_t ATTACK_STATUS_OFFHAND_CRITICAL_BLOCK = -43;
constexpr int8_t ATTACK_STATUS_OFFHAND_CRITICAL_RESIST = -41;
constexpr int8_t ATTACK_STATUS_OFFHAND_CRITICAL = -37;

/**
 * The nine values the short of SM_ATTACK.java:57-85 can take. The first four arms of that switch look only at the **first** result's
 * AttackStatus ("Counter skills"): block or critical block 32, parry or critical parry 64, dodge or critical dodge 128, resist or physical
 * critical resist 256. Every other status falls to the `default:` arm, which writes 0 without a critical proc effect and, with one, 1 or 2 for
 * a Player target and 1025 or 1026 for any other target (1 and 1025 mean the proc skill is 8218).
 */
constexpr uint16_t ATTACK_COUNTER_NONE = 0;
constexpr uint16_t ATTACK_COUNTER_BLOCK = 32;
constexpr uint16_t ATTACK_COUNTER_PARRY = 64;
constexpr uint16_t ATTACK_COUNTER_DODGE = 128;
constexpr uint16_t ATTACK_COUNTER_RESIST = 256;
constexpr uint16_t ATTACK_PROC_PLAYER_TARGET_SKILL_8218 = 1;
constexpr uint16_t ATTACK_PROC_PLAYER_TARGET = 2;
constexpr uint16_t ATTACK_PROC_OTHER_TARGET_SKILL_8218 = 1025;
constexpr uint16_t ATTACK_PROC_OTHER_TARGET = 1026;

/** the shield tail of one AttackResult (SM_ATTACK.java:116-144); only the fields the arm of `shieldType` wrote are set */
struct AttackShieldInfo {
	int32_t protectorId = 0;
	int32_t protectedDamage = 0;
	int32_t protectedSkillId = 0;
	int32_t reflectedDamage = 0;
	int32_t reflectedSkillId = 0;
	int32_t mpAbsorbed = 0;

	bool operator==(const AttackShieldInfo&) const = default;
};

/** one entry of the attack list (SM_ATTACK.java:106-145) */
struct AttackResultEntry {
	/** AttackResult.getDamage(), **unclamped** - the gate's A4 (ii) sums these and compares them against the applied damage */
	int32_t damage = 0;
	/** AttackStatus.getId(), signed */
	int8_t attackStatusId = 0;
	/** (byte) AttackResult.getShieldType(): 1 reflector, 2 normal shield, 8 protect effect; it selects the tail below */
	int8_t shieldType = 0;
	AttackShieldInfo shield;

	bool operator==(const AttackResultEntry&) const = default;
};

/** Effect.getTargetX/Y/Z of the critical proc effect (SM_ATTACK.java:93-97) */
struct CriticalProcPosition {
	float x = 0, y = 0, z = 0;

	bool operator==(const CriticalProcPosition&) const = default;
};

/**
 * The one branch of SM_ATTACK the bytes cannot always name. SM_ATTACK.java:93-97 writes the proc effect's three target floats whenever
 * `criticalProcEffect != null`, whichever arm of the counter switch ran, so the flag decides for eight of its nine values (0 is the `default:`
 * arm without a proc, 1/2/1025/1026 the same arm with one) but not for the four counter constants, which are written in arms that never look
 * at the proc.
 *
 * **Java reaches that combination from nowhere**, which is why the default is `false` and not an exception: the only construction site of
 * SM_ATTACK in the whole game server is CreatureController.attackTarget (CreatureController.java:348), and it creates a proc effect only when
 * `AttackStatus.getBaseStatus(attackResult.getFirst().getAttackStatus()) == AttackStatus.CRITICAL` (CreatureController.java:340-342).
 * CRITICAL is -54 and falls to SM_ATTACK's `default:` arm, so a counter flag and a proc effect cannot occur together. The flag is kept as an
 * option rather than hardcoded because that argument is about the *callers* of writeImpl, and D9 asks this file to model writeImpl.
 */
struct AttackOptions {
	bool criticalProcEffectWithCounterStatus = false;
};

/** SM_ATTACK (SM_ATTACK.java:44-147) */
struct Attack {
	int32_t attackerObjectId = 0;
	/** CreatureGameStats.getAttackCounter() truncated to a byte by writeC */
	uint8_t attackNo = 0;
	/** the delay CreatureController.attackTarget was called with, truncated to a short by writeH */
	uint16_t time = 0;
	uint8_t attackTypeAnimation = 0;
	uint8_t attackHandAnimation = 0;
	int32_t targetObjectId = 0;
	uint8_t targetHpPercentage = 0;
	uint8_t attackerHpPercentage = 0;
	/** the "Counter skills" short of :57-85, one of the nine ATTACK_COUNTER_* / ATTACK_PROC_* values */
	uint16_t counterSkillFlag = 0;
	/** set exactly when the packet carried a critical proc effect */
	std::optional<CriticalProcPosition> criticalProcPosition;
	/** never empty: SM_ATTACK.java:57 dereferences attackList.get(0), so Java throws before writing a zero-length list */
	std::vector<AttackResultEntry> results;
};

Attack decodeAttack(std::span<const uint8_t> body, const AttackOptions& options = {});

// ---- SM_ATTACK_STATUS -------------------------------------------------------------------------------------------------------------------

/** SM_ATTACK_STATUS.TYPE (SM_ATTACK_STATUS.java:21-66), the byte :153 writes; the gate's A3 and A6 read REGULAR and NATURAL_HP */
constexpr uint8_t ATTACK_STATUS_TYPE_NATURAL_HP = 3;
constexpr uint8_t ATTACK_STATUS_TYPE_USED_HP = 4;
constexpr uint8_t ATTACK_STATUS_TYPE_REGULAR = 5;
constexpr uint8_t ATTACK_STATUS_TYPE_ABSORBED_HP = 6;
constexpr uint8_t ATTACK_STATUS_TYPE_DAMAGE = 7;
constexpr uint8_t ATTACK_STATUS_TYPE_DELAYDAMAGE = 10;
constexpr uint8_t ATTACK_STATUS_TYPE_MP = 21;
constexpr uint8_t ATTACK_STATUS_TYPE_NATURAL_MP = 22;

/** SM_ATTACK_STATUS.LOG (SM_ATTACK_STATUS.java:68-98), the byte :156 writes; the melee auto-attack path uses REGULAR */
constexpr uint8_t ATTACK_STATUS_LOG_REGULAR = 191;

/** SM_ATTACK_STATUS.CRITICAL_DISPLAY_CODE (SM_ATTACK_STATUS.java:12), the only non-zero value of the last byte */
constexpr uint8_t ATTACK_STATUS_CRITICAL_DISPLAY_CODE = 12;

/** SM_ATTACK_STATUS (SM_ATTACK_STATUS.java:122-158), a fixed 14-byte body */
struct AttackStatusUpdate {
	int32_t creatureObjectId = 0;
	/**
	 * The int as it is on the wire. The switch of :125-152 negates the value for the damage and cost types (DAMAGE, DELAYDAMAGE, FALL_DAMAGE,
	 * FP_DAMAGE, MAGICCOUNTERATK, DISPELBUFFCOUNTERATK, USED_HP, DROWNING, USED_MP, DAMAGE_MP) and writes it unchanged for every other type, so
	 * a REGULAR melee hit - which takes the `default:` arm - carries its damage **positive** (m5b-plan.md §6.3 A4).
	 */
	int32_t value = 0;
	uint8_t type = 0;
	/**
	 * The HP percentage for every arm except the MP ones (USED_MP, DAMAGE_MP, MP, NATURAL_MP, HEAL_MP, ABSORBED_MP), which write the MP
	 * percentage into the same byte. It is a **percentage**, never an absolute value: A3 (ii) reads it, A4 does not.
	 */
	uint8_t hpOrMp = 0;
	uint16_t skillId = 0;
	uint8_t logId = 0;
	bool criticalHit = false;
};

AttackStatusUpdate decodeAttackStatus(std::span<const uint8_t> body);

// ---- SM_ATTACK_RESPONSE -----------------------------------------------------------------------------------------------------------------

/** the six messages the factory methods of SM_ATTACK_RESPONSE.java:11-37 can produce (there is no factory for 3, "unk") */
constexpr uint8_t ATTACK_RESPONSE_TARGET_IN_DIFFERENT_AREA = 1;
constexpr uint8_t ATTACK_RESPONSE_STOP_INVALID_TARGET = 2;
constexpr uint8_t ATTACK_RESPONSE_TARGET_TOO_FAR_AWAY = 4;
constexpr uint8_t ATTACK_RESPONSE_STOP_OBSTACLE_IN_THE_WAY = 5;
constexpr uint8_t ATTACK_RESPONSE_STOP_TOO_CLOSE_TO_ATTACK = 6;
constexpr uint8_t ATTACK_RESPONSE_STOP_WITHOUT_MESSAGE = 7;

/** SM_ATTACK_RESPONSE (SM_ATTACK_RESPONSE.java:45-48), a 2-byte body */
struct AttackResponse {
	uint8_t message = 0;
	/** the attack counter the rejected CM_ATTACK carried, truncated to a byte by writeC */
	uint8_t attackCount = 0;
};

AttackResponse decodeAttackResponse(std::span<const uint8_t> body);

// ---- SM_EMOTION -------------------------------------------------------------------------------------------------------------------------

/**
 * EmotionType.DIE (model/EmotionType.java:29). The four EmoteManager ids an npc broadcasts (EMOTION_WALK, EMOTION_CHANGE_SPEED,
 * EMOTION_ATTACKMODE_IN_MOVE, EMOTION_NEUTRALMODE_IN_MOVE) are declared in PacketDecoders.h; the complete EmotionType table lives in
 * CombatDecoders.cpp, where the arm of SM_EMOTION's switch is looked up.
 */
constexpr uint8_t EMOTION_DIE = 18;

/**
 * SM_EMOTION (SM_EMOTION.java:93-181), all 55 emotion types. The header (:94-97) is always there; which of the remaining fields are set
 * depends on the arm of the switch the emotion type selects, and every field this decoder does not fill keeps its zero.
 */
struct Emotion {
	int32_t senderObjectId = 0;
	/** EmotionType.getTypeId() */
	uint8_t emotionType = 0;
	/** Creature.getState(), truncated to a short by writeH */
	uint16_t state = 0;
	/** CreatureGameStats.getMovementSpeedFloat() */
	float speed = 0;
	/**
	 * The `targetObjectId` field of the packet: the last attacker for DIE and the looter for the four LOOT emotions (:128-134), the windstream
	 * distance for WINDSTREAM (:149), the emote target for EMOTE (:166), the ride id for RIDE / RIDE_END (:153-155) and the optional trailing
	 * id of the `default:` arm (:176-179). For DIE it is 0 only when the creature killed itself (CreatureController.java:164-165), so the
	 * gate's D1a reads the player's object id out of it.
	 */
	int32_t targetObjectId = 0;
	/** the `emotion` field: the teleport id of START_FLYTELEPORT and WINDSTREAM (:144, :148) and the emote id of EMOTE (:167) */
	int32_t emotion = 0;
	/** CHAIR_SIT and CHAIR_UP only (:135-141) */
	float x = 0, y = 0, z = 0;
	uint8_t heading = 0;
	/** CHANGE_SPEED only (:170-175): Stat2.getBase() and getCurrent() of the attack speed */
	uint16_t baseAttackSpeed = 0, currentAttackSpeed = 0;
};

/**
 * The full SM_EMOTION body, for every emotion type. `decodeEmotionHeader` of PacketDecoders.h stays what it is - the M5a async set's prefix
 * reader, which consumes the body exactly only for the four EmoteManager ids; this one consumes it exactly for all of them.
 *
 * Two arms are written conditionally on a value the wire does not carry (`if (targetObjectId != 0) writeD(targetObjectId)` at :153 and :177),
 * so for RIDE / RIDE_END and for the `default:` arm the decoder resolves the branch from the body length, which is exact in both cases: the
 * ride arm is 12 bytes without the id and 16 with it, the default arm 0 or 4.
 */
Emotion decodeEmotion(std::span<const uint8_t> body);

// ---- SM_DIE -----------------------------------------------------------------------------------------------------------------------------

/** SM_DIE (SM_DIE.java:31-37), an 8-byte body; the gate's P1 waits for it about 500 ms after the character's SM_EMOTION(DIE) */
struct Die {
	/** InstanceHandler.allowSelfReviveBySkill() && Player.canUseRebirthRevive() */
	bool allowReviveBySkill = false;
	/** InstanceHandler.allowSelfReviveByItem() && Player.haveSelfRezItem() */
	bool allowReviveByItem = false;
	/** Kisk.getRemainingLifetime(), 0 without a kisk or when the instance forbids kisk revive */
	int32_t remainingKiskTimeSeconds = 0;
	/** InstanceHandler.allowInstanceRevive(): 0 selects ReviveType.BIND_REVIVE, 1 the instance revive */
	bool allowInstanceRevive = false;
	/** the invasion world flag, written as 0x80 or 0x00 */
	bool invasion = false;
};

Die decodeDie(std::span<const uint8_t> body);

// ---- SM_LOOT_STATUS ---------------------------------------------------------------------------------------------------------------------

/** SM_LOOT_STATUS.Status (SM_LOOT_STATUS.java:38-53) */
constexpr uint8_t LOOT_STATUS_LOOT_ENABLE = 0;
constexpr uint8_t LOOT_STATUS_LOOT_DISABLE = 1;
constexpr uint8_t LOOT_STATUS_OPEN_DROP_LIST = 2;
constexpr uint8_t LOOT_STATUS_CLOSE_DROP_LIST = 3;

/** SM_LOOT_STATUS (SM_LOOT_STATUS.java:27-31), a 9-byte body; R3's successor when M5b-3 lands */
struct LootStatus {
	int32_t targetObjectId = 0;
	uint8_t status = 0;
	/** DropItem.getLootEffectId() of the first drop that has one, and 0 for every status but LOOT_ENABLE (:23) */
	int32_t lootEffectId = 0;
};

LootStatus decodeLootStatus(std::span<const uint8_t> body);

// ---- SM_STATUPDATE_HP -------------------------------------------------------------------------------------------------------------------

/**
 * SM_STATUPDATE_HP (SM_STATUPDATE_HP.java:26-29), an 8-byte body of two ints. **Absolute** HP, not a percentage - which is what makes the
 * gate's A6 possible at all, since SM_ATTACK_STATUS carries only a percentage.
 */
struct StatUpdateHp {
	int32_t currentHp = 0;
	int32_t maxHp = 0;
};

StatUpdateHp decodeStatUpdateHp(std::span<const uint8_t> body);

} // namespace aion::gameserver::scenario::decoders
