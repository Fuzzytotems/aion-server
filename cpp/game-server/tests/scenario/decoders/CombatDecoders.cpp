#include "decoders/CombatDecoders.h"

#include <bit>
#include <string>

namespace aion::gameserver::scenario::decoders {

namespace {

/**
 * Reads a float and fails unless it is bit-identical to value (a literal Java writeF constant). The comparison is on the bit pattern, not on
 * the float, because the constants it guards are exact and `==` on floats is a warning in some builds.
 */
void expectF(BodyReader& reader, float value, std::string_view what) {
	const size_t start = reader.offset();
	if (const float read = reader.F(); std::bit_cast<uint32_t>(read) != std::bit_cast<uint32_t>(value))
		reader.fail(std::string(what) + ": expected " + std::to_string(value) + ", got " + std::to_string(read) + ", field at offset " +
			std::to_string(start));
}

/** reads a byte and fails unless it is 0 or 1 (a Java `flag ? 1 : 0`) */
bool readFlag(BodyReader& reader, std::string_view what) {
	const uint8_t value = reader.C();
	if (value > 1)
		reader.fail(std::string(what) + ": expected 0 or 1, got " + std::to_string(value));
	return value == 1;
}

// ---- SM_ATTACK --------------------------------------------------------------------------------------------------------------------------

/** SM_ATTACK.java:116-144: the tail the shieldType of one AttackResult selects */
void readShieldInfo(BodyReader& reader, AttackResultEntry& entry) {
	switch (entry.shieldType) {
		case 0:
		case 2:
			break; // :117-119, nothing follows
		case 8:
		case 10:
			entry.shield.protectorId = reader.D();      // :122
			entry.shield.protectedDamage = reader.D();  // :123
			entry.shield.protectedSkillId = reader.D(); // :124
			break;
		case 16:
			reader.expectZeros(20, "the five zero ints of SM_ATTACK's shieldType 16 arm (SM_ATTACK.java:127-131)");
			entry.shield.mpAbsorbed = reader.D();       // :132
			entry.shield.reflectedSkillId = reader.D(); // :133
			break;
		default:
			entry.shield.protectorId = reader.D();      // :136
			entry.shield.protectedDamage = reader.D();  // :137
			entry.shield.protectedSkillId = reader.D(); // :138
			entry.shield.reflectedDamage = reader.D();  // :139
			entry.shield.reflectedSkillId = reader.D(); // :140
			reader.expectZeros(8, "the two zero ints that close SM_ATTACK's default shield arm (SM_ATTACK.java:141-142)");
			break;
	}
}

// ---- SM_EMOTION -------------------------------------------------------------------------------------------------------------------------

/** the ten arms of the switch of SM_EMOTION.java:98-180 */
enum class EmotionArm {
	/** :99-127, a bare break: the header is the whole body */
	EMPTY,
	/** :128-134, DIE and the four LOOT emotions: writeD(targetObjectId) */
	TARGET,
	/** :135-141, CHAIR_SIT and CHAIR_UP: three floats and the heading */
	CHAIR,
	/** :142-145, START_FLYTELEPORT: writeD(emotion), the teleport id */
	FLY_TELEPORT,
	/** :146-150, WINDSTREAM: the teleport id and the distance */
	WINDSTREAM,
	/** :151-159, RIDE and RIDE_END: the ride id only when it is not 0, then three float constants */
	RIDE,
	/** :160-163, RESURRECT: writeD(0) */
	RESURRECT,
	/** :164-169, EMOTE: the target, the emote id and a literal 1 */
	EMOTE,
	/** :170-175, CHANGE_SPEED: the two attack speeds and a literal 0 */
	CHANGE_SPEED,
	/** :176-179, everything else: the target object id only when it is not 0 */
	TRAILING_TARGET,
};

/**
 * The arm SM_EMOTION's switch takes for an EmotionType id (model/EmotionType.java:6-62). std::nullopt for a byte that is not an EmotionType
 * id at all - 20, 23, 28, 29 and 30 are the gaps the Java enum leaves, and everything above 54 except 255 (NONE, -1) is not an id either.
 */
std::optional<EmotionArm> emotionArm(uint8_t typeId) {
	switch (typeId) {
		case 0:  // SELECT_TARGET
		case 1:  // JUMP
		case 2:  // SIT
		case 3:  // STAND
		case 7:  // LAND_FLYTELEPORT
		case 9:  // WINDSTREAM_END
		case 10: // WINDSTREAM_EXIT
		case 11: // WINDSTREAM_START_BOOST
		case 12: // WINDSTREAM_END_BOOST
		case 13: // FLY
		case 14: // LAND
		case 24: // ATTACKMODE_IN_MOVE
		case 25: // NEUTRALMODE_IN_MOVE
		case 26: // WALK
		case 27: // RUN
		case 31: // OPEN_DOOR
		case 32: // CLOSE_DOOR
		case 33: // OPEN_PRIVATESHOP
		case 34: // CLOSE_PRIVATESHOP
		case 36: // POWERSHARD_ON
		case 37: // POWERSHARD_OFF
		case 38: // ATTACKMODE_IN_STANDING
		case 39: // NEUTRALMODE_IN_STANDING
		case 47: // STOP_GLIDE
		case 48: // STOP_FLY
		case 50: // START_FEEDING
		case 51: // END_FEEDING
		case 52: // WINDSTREAM_STRAFE
			return EmotionArm::EMPTY;
		case 18: // DIE
		case 40: // START_LOOT
		case 41: // END_LOOT
		case 42: // START_QUESTLOOT
		case 43: // END_QUESTLOOT
			return EmotionArm::TARGET;
		case 4: // CHAIR_SIT
		case 5: // CHAIR_UP
			return EmotionArm::CHAIR;
		case 6: // START_FLYTELEPORT
			return EmotionArm::FLY_TELEPORT;
		case 8: // WINDSTREAM
			return EmotionArm::WINDSTREAM;
		case 15: // RIDE
		case 16: // RIDE_END
			return EmotionArm::RIDE;
		case 19: // RESURRECT
			return EmotionArm::RESURRECT;
		case 21: // EMOTE
			return EmotionArm::EMOTE;
		case 35: // CHANGE_SPEED
			return EmotionArm::CHANGE_SPEED;
		case 17:  // ATTACK
		case 22:  // EMOTE_END
		case 44:  // TURN_RIGHT
		case 45:  // TURN_LEFT
		case 46:  // START_GLIDE
		case 49:  // SUMMON_STOP_JUMP
		case 53:  // START_SPRINT
		case 54:  // END_SPRINT
		case 255: // NONE (-1, written by writeC as 0xFF)
			return EmotionArm::TRAILING_TARGET;
		default:
			return std::nullopt;
	}
}

} // namespace

// ---- SM_ATTACK --------------------------------------------------------------------------------------------------------------------------

Attack decodeAttack(std::span<const uint8_t> body, const AttackOptions& options) {
	BodyReader reader(body, "SM_ATTACK");
	Attack attack;
	attack.attackerObjectId = reader.D();     // SM_ATTACK.java:45
	attack.attackNo = reader.C();             // :46
	attack.time = reader.H();                 // :47
	attack.attackTypeAnimation = reader.C();  // :48
	attack.attackHandAnimation = reader.C();  // :49
	attack.targetObjectId = reader.D();       // :51
	attack.targetHpPercentage = reader.C();   // :53
	attack.attackerHpPercentage = reader.C(); // :54
	attack.counterSkillFlag = reader.H();     // :57-85, the "Counter skills" switch

	bool criticalProcEffect = false;
	switch (attack.counterSkillFlag) {
		case ATTACK_COUNTER_NONE:
			break; // :82, the default arm without a proc effect
		case ATTACK_PROC_PLAYER_TARGET_SKILL_8218:
		case ATTACK_PROC_PLAYER_TARGET:
		case ATTACK_PROC_OTHER_TARGET_SKILL_8218:
		case ATTACK_PROC_OTHER_TARGET:
			criticalProcEffect = true; // :76-80, the default arm with one
			break;
		case ATTACK_COUNTER_BLOCK:
		case ATTACK_COUNTER_PARRY:
		case ATTACK_COUNTER_DODGE:
		case ATTACK_COUNTER_RESIST:
			criticalProcEffect = options.criticalProcEffectWithCounterStatus; // :59-74, arms that do not look at the proc effect
			break;
		default:
			reader.fail("the counter skill short is " + std::to_string(attack.counterSkillFlag) +
				", which is none of the nine values SM_ATTACK.java:57-85 can write");
	}

	reader.expectH(0, "the short after the counter skill flag (SM_ATTACK.java:92)");
	if (criticalProcEffect) {
		CriticalProcPosition position;
		position.x = reader.F(); // :94
		position.y = reader.F(); // :95
		position.z = reader.F(); // :96
		attack.criticalProcPosition = position;
	}

	const uint8_t resultCount = reader.C(); // :105, attackList.size()
	if (resultCount == 0)
		reader.fail("an empty attack list: SM_ATTACK.java:57 reads attackList.get(0) before writing anything, so Java cannot send one");
	for (uint8_t i = 0; i < resultCount; i++) {
		AttackResultEntry entry;
		entry.damage = reader.D();          // :107
		entry.attackStatusId = reader.Cs(); // :108
		entry.shieldType = reader.Cs();     // :110-111
		readShieldInfo(reader, entry);
		attack.results.push_back(entry);
	}
	reader.expectC(0, "the byte that closes SM_ATTACK (SM_ATTACK.java:146)");
	reader.expectFullyConsumed();
	return attack;
}

// ---- SM_ATTACK_STATUS -------------------------------------------------------------------------------------------------------------------

AttackStatusUpdate decodeAttackStatus(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_ATTACK_STATUS");
	AttackStatusUpdate update;
	update.creatureObjectId = reader.D(); // SM_ATTACK_STATUS.java:124
	update.value = reader.D();            // :125-152, negated by the damage and cost arms
	update.type = reader.C();             // :153
	update.hpOrMp = reader.C();           // :154
	update.skillId = reader.H();          // :155
	update.logId = reader.C();            // :156
	const uint8_t critical = reader.C();  // :157, criticalHit ? CRITICAL_DISPLAY_CODE : 0
	if (critical != 0 && critical != ATTACK_STATUS_CRITICAL_DISPLAY_CODE)
		reader.fail("the critical hit byte is " + std::to_string(critical) + ", but SM_ATTACK_STATUS.java:157 writes only 0 or " +
			std::to_string(ATTACK_STATUS_CRITICAL_DISPLAY_CODE));
	update.criticalHit = critical == ATTACK_STATUS_CRITICAL_DISPLAY_CODE;
	reader.expectFullyConsumed();
	return update;
}

// ---- SM_ATTACK_RESPONSE -----------------------------------------------------------------------------------------------------------------

AttackResponse decodeAttackResponse(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_ATTACK_RESPONSE");
	AttackResponse response;
	response.message = reader.C(); // SM_ATTACK_RESPONSE.java:46
	switch (response.message) {
		case ATTACK_RESPONSE_TARGET_IN_DIFFERENT_AREA:
		case ATTACK_RESPONSE_STOP_INVALID_TARGET:
		case ATTACK_RESPONSE_TARGET_TOO_FAR_AWAY:
		case ATTACK_RESPONSE_STOP_OBSTACLE_IN_THE_WAY:
		case ATTACK_RESPONSE_STOP_TOO_CLOSE_TO_ATTACK:
		case ATTACK_RESPONSE_STOP_WITHOUT_MESSAGE:
			break;
		default:
			// the constructor is private and only the six factory methods of SM_ATTACK_RESPONSE.java:11-37 reach it
			reader.fail("message " + std::to_string(response.message) + " is not one of the six SM_ATTACK_RESPONSE.java:11-37 can produce");
	}
	response.attackCount = reader.C(); // :47
	reader.expectFullyConsumed();
	return response;
}

// ---- SM_EMOTION -------------------------------------------------------------------------------------------------------------------------

Emotion decodeEmotion(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_EMOTION");
	Emotion decoded;
	decoded.senderObjectId = reader.D(); // SM_EMOTION.java:94
	decoded.emotionType = reader.C();    // :95
	decoded.state = reader.H();          // :96
	decoded.speed = reader.F();          // :97

	const std::optional<EmotionArm> arm = emotionArm(decoded.emotionType);
	if (!arm)
		reader.fail("emotion type " + std::to_string(decoded.emotionType) + " is not an EmotionType id (model/EmotionType.java:6-62)");
	switch (*arm) {
		case EmotionArm::EMPTY:
			break;
		case EmotionArm::TARGET:
			decoded.targetObjectId = reader.D(); // :133
			break;
		case EmotionArm::CHAIR:
			decoded.x = reader.F();       // :137
			decoded.y = reader.F();       // :138
			decoded.z = reader.F();       // :139
			decoded.heading = reader.C(); // :140
			break;
		case EmotionArm::FLY_TELEPORT:
			decoded.emotion = reader.D(); // :144, the teleport id
			break;
		case EmotionArm::WINDSTREAM:
			decoded.emotion = reader.D();        // :148, the teleport id
			decoded.targetObjectId = reader.D(); // :149, the distance
			break;
		case EmotionArm::RIDE:
			// :153-155 writes the ride id only when it is not 0, so the arm is 16 bytes with it and 12 without
			if (reader.remaining() == 16)
				decoded.targetObjectId = reader.D();
			else if (reader.remaining() != 12)
				reader.fail("the RIDE arm of SM_EMOTION.java:151-159 is 12 bytes without a ride id and 16 with one, not " +
					std::to_string(reader.remaining()));
			expectF(reader, 63.0f, "the first float constant of SM_EMOTION's RIDE arm (writeF(0x3F), SM_EMOTION.java:156)");
			expectF(reader, 63.0f, "the second float constant of SM_EMOTION's RIDE arm (writeF(0x3F), SM_EMOTION.java:157)");
			expectF(reader, 64.0f, "the third float constant of SM_EMOTION's RIDE arm (writeF(0x40), SM_EMOTION.java:158)");
			break;
		case EmotionArm::RESURRECT:
			reader.expectD(0, "the int of SM_EMOTION's RESURRECT arm (SM_EMOTION.java:162)");
			break;
		case EmotionArm::EMOTE:
			decoded.targetObjectId = reader.D(); // :166
			decoded.emotion = reader.H();        // :167
			reader.expectC(1, "the byte that closes SM_EMOTION's EMOTE arm (SM_EMOTION.java:168)");
			break;
		case EmotionArm::CHANGE_SPEED:
			decoded.baseAttackSpeed = reader.H();    // :172
			decoded.currentAttackSpeed = reader.H(); // :173
			reader.expectC(0, "the byte that closes SM_EMOTION's CHANGE_SPEED arm (SM_EMOTION.java:174, \"new 4.0\")");
			break;
		case EmotionArm::TRAILING_TARGET:
			// :177-179 writes the target object id only when it is not 0
			if (reader.remaining() == 4)
				decoded.targetObjectId = reader.D();
			break;
	}
	reader.expectFullyConsumed();
	return decoded;
}

// ---- SM_DIE -----------------------------------------------------------------------------------------------------------------------------

Die decodeDie(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_DIE");
	Die die;
	die.allowReviveBySkill = readFlag(reader, "allowReviveBySkill (SM_DIE.java:32)");
	die.allowReviveByItem = readFlag(reader, "allowReviveByItem (SM_DIE.java:33)");
	die.remainingKiskTimeSeconds = reader.D(); // :34
	die.allowInstanceRevive = readFlag(reader, "allowInstanceRevive (SM_DIE.java:35)");
	const uint8_t invasion = reader.C(); // :36, invasion ? 0x80 : 0x00
	if (invasion != 0x00 && invasion != 0x80)
		reader.fail("the invasion byte is " + std::to_string(invasion) + ", but SM_DIE.java:36 writes only 0x00 or 0x80");
	die.invasion = invasion == 0x80;
	reader.expectFullyConsumed();
	return die;
}

// ---- SM_LOOT_STATUS ---------------------------------------------------------------------------------------------------------------------

LootStatus decodeLootStatus(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_LOOT_STATUS");
	LootStatus loot;
	loot.targetObjectId = reader.D(); // SM_LOOT_STATUS.java:28
	loot.status = reader.C();         // :29
	if (loot.status > LOOT_STATUS_CLOSE_DROP_LIST)
		reader.fail("status " + std::to_string(loot.status) + " is not one of the four of SM_LOOT_STATUS.java:38-53");
	loot.lootEffectId = reader.D(); // :30
	reader.expectFullyConsumed();
	return loot;
}

// ---- SM_STATUPDATE_HP -------------------------------------------------------------------------------------------------------------------

StatUpdateHp decodeStatUpdateHp(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_STATUPDATE_HP");
	StatUpdateHp stat;
	stat.currentHp = reader.D(); // SM_STATUPDATE_HP.java:27
	stat.maxHp = reader.D();     // :28
	reader.expectFullyConsumed();
	return stat;
}

} // namespace aion::gameserver::scenario::decoders
