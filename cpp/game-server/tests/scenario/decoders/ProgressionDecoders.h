#pragma once

// The progression packets the M5e gate reads (m5e-plan.md §2.11, G-02; the §10.3 rows X1-X17): the level-up and class-change animation, the
// removed skill, DP, the resurrection offer, the mantra, the robot, the flight time, the chat-window message of `sendMessage`, and the summon
// panel, its update, its removal and the owner's removal. SM_SKILL_LIST, SM_PLAYER_INFO, SM_STATS_INFO and SM_QUEST_COMPLETED_LIST are M5a's
// (PacketDecoders.h), SM_QUEST_ACTION and SM_STATUPDATE_EXP M5d's (QuestDecoders.h), SM_DIALOG_WINDOW M5c's (EconomyDecoders.h), the cast and
// effect packets M5b-2's (SkillDecoders.h).
//
// **m5a-plan.md D9:** every layout is written from the Java `writeImpl` under game-server/src/com/aionemu/gameserver/network/aion/serverpackets/
// (SM_ACTION_ANIMATION.java, SM_SKILL_REMOVE.java, SM_STATUPDATE_DP.java, SM_DP_INFO.java, SM_RESURRECT.java, SM_MANTRA_EFFECT.java,
// SM_RIDE_ROBOT.java, SM_FLY_TIME.java, SM_MESSAGE.java, SM_SUMMON_PANEL.java, SM_SUMMON_UPDATE.java, SM_SUMMON_PANEL_REMOVE.java,
// SM_SUMMON_OWNER_REMOVE.java) and the enums they write from model/animations/ActionAnimation.java and model/ChatType.java. Nothing here
// includes, calls or mirrors a C++ serverpackets header.
//
// Every decode function consumes the body exactly and throws DecodeError otherwise; the Java constants that carry no data are verified.

#include <cstdint>
#include <span>
#include <string>

#include "decoders/PacketDecoders.h" // BodyReader, DecodeError

namespace aion::gameserver::scenario::decoders {

// ---- SM_ACTION_ANIMATION -------------------------------------------------------------------------------------------------------------------

/**
 * ActionAnimation ids (ActionAnimation.java:12-17): LEVEL_UP 0, what PlayerController.onLevelChange broadcasts (PlayerController.java:587);
 * CLASS_CHANGE 4, what ClassChangeService.setClass broadcasts (ClassChangeService.java:75) - and CRAFT_LEVEL_UP is 4 too, so a 4 names the id,
 * not the cause
 */
constexpr uint16_t ACTION_ANIMATION_LEVEL_UP = 0;
constexpr uint16_t ACTION_ANIMATION_CLASS_CHANGE = 4;

/** SM_ACTION_ANIMATION (SM_ACTION_ANIMATION.java:27-31): writeD(targetObjectId), writeH(actionAnimation.getId()), writeD(levelOrObjectId) */
struct ActionAnimation {
	int32_t objectId = 0;
	uint16_t animation = 0;
	/** the new level of LEVEL_UP and CLASS_CHANGE, an object id for other animations, 0 for the two-argument constructor */
	int32_t levelOrObjectId = 0;
};

ActionAnimation decodeActionAnimation(std::span<const uint8_t> body);

// ---- SM_SKILL_REMOVE -----------------------------------------------------------------------------------------------------------------------

/**
 * SM_SKILL_REMOVE (SM_SKILL_REMOVE.java:23-27): writeH(skillId), writeC(skillLevel), writeC(skillType). The level byte of a profession skill is
 * PlayerSkillEntry.getProfessionFlag() - 1 for a tapping or the morph skill, the current xp of a crafting skill (PlayerSkillEntry.java:84-90) -
 * and the skill level otherwise; the type is 0 normal, 1 stigma, 3 linked stigma (PlayerSkillEntry.java:17)
 */
struct SkillRemove {
	uint16_t skillId = 0;
	uint8_t levelOrFlag = 0;
	uint8_t skillType = 0;
};

SkillRemove decodeSkillRemove(std::span<const uint8_t> body);

// ---- DP --------------------------------------------------------------------------------------------------------------------------------

/** SM_STATUPDATE_DP (SM_STATUPDATE_DP.java:23-25): writeH(currentDp), to the player alone (PlayerCommonData.setDp, PlayerCommonData.java:473) */
uint16_t decodeStatUpdateDp(std::span<const uint8_t> body);

/** SM_DP_INFO (SM_DP_INFO.java:20-23): writeD(playerObjectId), writeH(currentDp), broadcast with the player (PlayerCommonData.java:471) */
struct DpInfo {
	int32_t objectId = 0;
	uint16_t currentDp = 0;
};

DpInfo decodeDpInfo(std::span<const uint8_t> body);

// ---- SM_RESURRECT --------------------------------------------------------------------------------------------------------------------------

/** SM_RESURRECT (SM_RESURRECT.java:25-29): writeS(the resurrector's name), writeH(skillId), writeD(0) */
struct Resurrect {
	std::string name;
	uint16_t skillId = 0;
};

Resurrect decodeResurrect(std::span<const uint8_t> body);

// ---- SM_MANTRA_EFFECT ----------------------------------------------------------------------------------------------------------------------

/** SM_MANTRA_EFFECT (SM_MANTRA_EFFECT.java:21-25): writeD(0), writeD(effector object id), writeH(subEffectId) - the aura's skill id */
struct MantraEffect {
	int32_t effectorObjectId = 0;
	uint16_t subEffectId = 0;
};

MantraEffect decodeMantraEffect(std::span<const uint8_t> body);

// ---- SM_RIDE_ROBOT -------------------------------------------------------------------------------------------------------------------------

/** SM_RIDE_ROBOT (SM_RIDE_ROBOT.java:25-28): writeD(player object id), writeD(robotId) - 0 when the robot is left (RideRobotEffect.java:42-43) */
struct RideRobot {
	int32_t objectId = 0;
	int32_t robotId = 0;
};

RideRobot decodeRideRobot(std::span<const uint8_t> body);

// ---- SM_FLY_TIME ---------------------------------------------------------------------------------------------------------------------------

/** SM_FLY_TIME (SM_FLY_TIME.java:20-23): writeD(currentFp), writeD(maxFp) */
struct FlyTime {
	int32_t currentFp = 0;
	int32_t maxFp = 0;
};

FlyTime decodeFlyTime(std::span<const uint8_t> body);

// ---- SM_MESSAGE ---------------------------------------------------------------------------------------------------------------------------

/** ChatType ids (ChatType.java:15, 42): SHOUT, whose body carries the sender's position, and GOLDEN_YELLOW, PacketSendUtility.sendMessage's */
constexpr uint8_t CHAT_SHOUT = 3;
constexpr uint8_t CHAT_GOLDEN_YELLOW = 25;

/**
 * SM_MESSAGE (SM_MESSAGE.java:135-149): writeC(chatType id), writeC(the sender's race filter, 0 for a staff reader), writeD(senderObjectId),
 * writeS(senderName), writeS(message), and for SHOUT writeF(x), writeF(y), writeF(z). writeS(null) writes the terminator alone
 * (BaseServerPacket.java:119-122), so PacketSendUtility.sendMessage's `new SM_MESSAGE(0, null, msg, GOLDEN_YELLOW)` (PacketSendUtility.java:27-29)
 * decodes with an empty name
 */
struct Message {
	uint8_t chatType = 0;
	uint8_t senderRace = 0;
	int32_t senderObjectId = 0;
	std::string senderName;
	std::string message;
	/** SHOUT only */
	float x = 0, y = 0, z = 0;
};

Message decodeMessage(std::span<const uint8_t> body);

// ---- the summon packets --------------------------------------------------------------------------------------------------------------------

/**
 * SM_SUMMON_PANEL (SM_SUMMON_PANEL.java:20-32): writeD(objectId), writeH(level), writeD(0), writeD(0), writeD(current HP), writeD(max HP current),
 * writeD(main hand physical attack DISPLAY current), writeD(physical defence current), writeD(magical defence current), writeH(0),
 * writeD(Summon.getLiveTime())
 */
struct SummonPanel {
	int32_t objectId = 0;
	uint16_t level = 0;
	int32_t currentHp = 0, maxHp = 0;
	int32_t mainHandPAttack = 0, pDef = 0, mDef = 0;
	int32_t liveTime = 0;
};

SummonPanel decodeSummonPanel(std::span<const uint8_t> body);

/** SM_SUMMON_UPDATE (SM_SUMMON_UPDATE.java:21-80): the level, the mode, the stats' current values, then their base values */
struct SummonUpdate {
	uint8_t level = 0;
	/** SummonMode.getId() of the summon's mode */
	uint16_t mode = 0;
	int32_t currentHp = 0;
	int32_t maxHp = 0, mainHandPAttack = 0, pDef = 0;
	uint16_t mResist = 0;
	int32_t mDef = 0;
	uint16_t accuracy = 0, mainHandPCritical = 0, mBoost = 0, suppression = 0, mAccuracy = 0, mCritical = 0, parry = 0, evasion = 0;
	int32_t baseMaxHp = 0, baseMainHandPAttack = 0, basePDef = 0;
	uint16_t baseMResist = 0;
	int32_t baseMDef = 0;
	uint16_t baseAccuracy = 0, baseMainHandPCritical = 0, baseMBoost = 0, baseSuppression = 0, baseMAccuracy = 0, baseMCritical = 0, baseParry = 0,
	         baseEvasion = 0;
};

SummonUpdate decodeSummonUpdate(std::span<const uint8_t> body);

/** SM_SUMMON_PANEL_REMOVE (SM_SUMMON_PANEL_REMOVE.java:18-25): writeH(skillId), writeC(skillId != 0 ? 1 : 0) - the flag is verified */
uint16_t decodeSummonPanelRemove(std::span<const uint8_t> body);

/** SM_SUMMON_OWNER_REMOVE (SM_SUMMON_OWNER_REMOVE.java:18-20): writeD(summonObjId) */
int32_t decodeSummonOwnerRemove(std::span<const uint8_t> body);

} // namespace aion::gameserver::scenario::decoders
