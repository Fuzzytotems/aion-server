#pragma once

// The quest packets the M5d gate reads (m5d-plan.md §3.9, G-02; the §10.3 rows Y1, Y3, Y5-Y13): SM_QUEST_ACTION in all six of its action
// types, SM_NEARBY_QUESTS (the quest markers) and SM_STATUPDATE_EXP, which the M5b gate (R1 (b)) and the M5c gate (X15, X19) had each
// decoded in a file-local copy and which now lives here, once. SM_QUEST_LIST and SM_QUEST_COMPLETED_LIST are M5a's (PacketDecoders.h), the
// dialog window M5c's (EconomyDecoders.h: decodeDialogWindow), and the item packets of a reward M5b-3's (ItemDecoders.h). Since P6-Q prologue
// also SM_PLAY_MOVIE and the Java constants of a new character's prologue quests (decoders::Prologue), which every gate that creates one reads.
//
// **m5a-plan.md D9:** every layout is written from the Java `writeImpl` under game-server/src/com/aionemu/gameserver/network/aion/serverpackets/
// (SM_QUEST_ACTION.java, SM_NEARBY_QUESTS.java, SM_STATUPDATE_EXP.java, SM_PLAY_MOVIE.java), the status bytes from
// questEngine/model/QuestStatus.java and the quest id range from data/static_data/quest_data/quest_data.xml. Nothing here includes, calls or
// mirrors a C++ serverpackets header.
//
// Every decode function consumes the body exactly and throws DecodeError otherwise; the Java constants that carry no data are verified.

#include <cstdint>
#include <span>
#include <vector>

#include "decoders/PacketDecoders.h" // BodyReader, DecodeError

namespace aion::gameserver::scenario::decoders {

// ---- SM_QUEST_ACTION --------------------------------------------------------------------------------------------------------------------

/** SM_QUEST_ACTION.ActionType ids (SM_QUEST_ACTION.java:107-112): the first byte of every body but the empty one */
constexpr uint8_t QUEST_ACTION_ADD = 1;
constexpr uint8_t QUEST_ACTION_UPDATE = 2;
constexpr uint8_t QUEST_ACTION_ABANDON = 3;
constexpr uint8_t QUEST_ACTION_TIMER = 4;
constexpr uint8_t QUEST_ACTION_SHARE = 5;
constexpr uint8_t QUEST_ACTION_UNK = 6;

/**
 * QuestStatus.value() (QuestStatus.java:11-14), the status byte of ADD and UPDATE: never the ordinal. The decoder refuses the ordinals 0-2 of
 * START, REWARD and COMPLETE; LOCKED's ordinal 3 is START's value and cannot be told apart
 */
constexpr uint8_t QUEST_STATUS_START = 3;
constexpr uint8_t QUEST_STATUS_REWARD = 4;
constexpr uint8_t QUEST_STATUS_COMPLETE = 5;
constexpr uint8_t QUEST_STATUS_LOCKED = 6;

/**
 * SM_QUEST_ACTION (SM_QUEST_ACTION.java:67-104). The body is empty for a quest whose template has an extra_category other than NONE:
 * writeImpl returns before its first write (:69-71; m5d-plan.md §11 risk 9). quest_data.xml has 111 such quests, 1209 "[Spend Coin] Iron" the
 * first. Otherwise it is writeC(type), writeD(questId) and the arm of the type (:72-102):
 *
 * | type | arm | body |
 * |---|---|---|
 * | ADD (1) | writeC(status), writeC(0), writeD(step \| flags << 24), writeH(0), writeC(0) | 14 bytes |
 * | UPDATE (2) | writeC(status), writeC(0), writeD(step \| flags << 24), writeH(0) | 13 bytes |
 * | ABANDON (3) | writeD(0) | 9 bytes |
 * | TIMER (4) | writeD(timer), writeC(timer > 0 ? 1 : 0) | 10 bytes |
 * | SHARE (5) | writeD(sharerId), writeD(shareInAlliance ? 1 : 0) | 13 bytes |
 * | UNK (6) | writeH(1), writeH(0) | 9 bytes |
 *
 * The fields of an arm the body did not carry stay 0.
 */
struct QuestAction {
	/** the extra_category body: nothing was written, and every other field is 0 */
	bool empty = false;
	/** QUEST_ACTION_* */
	uint8_t actionType = 0;
	int32_t questId = 0;
	/** ADD, UPDATE: QuestStatus.value() (QUEST_STATUS_*); the decoder rejects any other value */
	uint8_t status = 0;
	/**
	 * ADD, UPDATE: the int as it is on the wire, `qs.getQuestVars().getQuestVars() | qs.getFlags() << 24` (:36-37, :78, :85). QuestVars packs
	 * six 6-bit vars, `Sum(var_i * 64^i)` (QuestVars.java:37-47), so vars 4 and 5 share the high byte with the flags; var(0) to var(3) are
	 * exact
	 */
	int32_t questVarsAndFlags = 0;
	/** TIMER: the seconds the client counts down (:92); 0 stops it, which CM_DELETE_QUEST sends for a timer quest (CM_DELETE_QUEST.java:34) */
	int32_t timer = 0;
	/** SHARE: the sharer's object id (:96) */
	int32_t sharerId = 0;
	/** SHARE: writeD(shareInAlliance ? 1 : 0), "0: group, 1: alliance" (:97); the decoder rejects any other value */
	bool shareInAlliance = false;

	/** quest var `index` (0-5) of questVarsAndFlags, 6 bits each; exact for 0-3 (see questVarsAndFlags) */
	int32_t var(int index) const noexcept { return (questVarsAndFlags >> (6 * index)) & 0x3F; }
	/** the high byte of questVarsAndFlags: the flags when vars 4 and 5 are 0 */
	uint8_t highByte() const noexcept { return static_cast<uint8_t>(static_cast<uint32_t>(questVarsAndFlags) >> 24); }

	bool operator==(const QuestAction&) const = default;
};

QuestAction decodeQuestAction(std::span<const uint8_t> body);

// ---- SM_NEARBY_QUESTS -------------------------------------------------------------------------------------------------------------------

/** SM_NEARBY_QUESTS.notYetAvailableBit (SM_NEARBY_QUESTS.java:14): the grey marker of a quest up to two levels too high */
constexpr int32_t NEARBY_QUEST_NOT_YET_AVAILABLE_BIT = 1 << 17;

/** one quest of SM_NEARBY_QUESTS */
struct NearbyQuest {
	/** the id without the marker bit */
	int32_t questId = 0;
	/**
	 * the marker bit was set: the map value, QuestService.getLevelRequirementDiff (PlayerController.java:170-177), was > 0
	 * (SM_NEARBY_QUESTS.java:27-28)
	 */
	bool notYetAvailable = false;
	/** the int as it is on the wire, `questId | notYetAvailableBit` for a grey quest (:28-29) */
	int32_t wire = 0;

	bool operator==(const NearbyQuest&) const = default;
};

/**
 * SM_NEARBY_QUESTS (SM_NEARBY_QUESTS.java:22-31): writeC(0), writeH(-size & 0xFFFF), then one writeD per entry of the map, in its iteration
 * order. The decoder rejects:
 * - a leading byte other than 0;
 * - a quest id twice (the entries are the keys of a Map);
 * - a wire int with a bit set above bit 17 or a negative one: quest_data.xml's highest id is 99002, below 2^17, so every id fits under the
 *   marker bit, and anything above it cannot be told from a corrupted body.
 */
struct NearbyQuests {
	/** in wire order (Java's HashMap iteration order; oracle.py m5d-quests `nearby.xmlOnlyWireOrder` models it) */
	std::vector<NearbyQuest> quests;

	/** the quest ids without the marker bit, in wire order */
	std::vector<int32_t> ids() const;
	/** the ids of the quests whose marker bit is set, in wire order */
	std::vector<int32_t> notYetAvailableIds() const;
	/**
	 * the wire ints in wire order. oracle.py m5d-quests lists `nearby.xmlOnlyWire` sorted, so it equals these sorted (compare them as sets);
	 * the oracle's model of the order is `nearby.xmlOnlyWireOrder.buckets`, one int per bucket where it is `exact`
	 */
	std::vector<int32_t> wireValues() const;
	bool contains(int32_t questId) const;
};

NearbyQuests decodeNearbyQuests(std::span<const uint8_t> body);

// ---- SM_STATUPDATE_EXP ------------------------------------------------------------------------------------------------------------------

/**
 * SM_STATUPDATE_EXP (SM_STATUPDATE_EXP.java:34-41): five writeQ, a 40-byte body. PlayerCommonData passes getExpShown(), getExpRecoverable(),
 * getExpNeed(), getCurrentReposeEnergy() and getMaxReposeEnergy() (PlayerCommonData.java:146, 287); the packet's own names for the last two
 * are curBoostExp and maxBoostExp, and those are the names here. Moved from M5bScenarioTest.cpp and M5cScenarioTest.cpp (m5d-plan.md G-02).
 */
struct StatUpdateExp {
	int64_t currentExp = 0;
	int64_t recoverableExp = 0;
	int64_t maxExp = 0;
	int64_t curBoostExp = 0;
	int64_t maxBoostExp = 0;

	bool operator==(const StatUpdateExp&) const = default;
};

StatUpdateExp decodeStatUpdateExp(std::span<const uint8_t> body);

// ---- SM_PLAY_MOVIE (P6-Q prologue) ------------------------------------------------------------------------------------------------------

/**
 * SM_PLAY_MOVIE (SM_PLAY_MOVIE.java:27-35): writeC(isCutsceneMovie ? 1 : 0), writeD(objectId), writeD(questId), writeD(cutsceneId), writeC(0),
 * writeC(canSkip ? 0 : 1), a 15-byte body. The decoder rejects a first or last byte other than 0 and 1 and an unknown byte other than 0.
 * AionClientPacketFactory's answer is CM_PLAY_MOVIE_END (GameSession::buildCM_PLAY_MOVIE_END), with the same type, object, quest and movie.
 */
struct PlayMovie {
	/** the first byte: 1 for a CutSceneMovie, 0 for a CutScene (:29) */
	bool cutsceneMovie = false;
	int32_t objectId = 0;
	int32_t questId = 0;
	int32_t movieId = 0;
	/** the last byte is 0 (:34) */
	bool canSkip = false;

	bool operator==(const PlayMovie&) const = default;
};

PlayMovie decodePlayMovie(std::span<const uint8_t> body);

// ---- the prologue of a new character (P6-Q prologue, owner answers 3 and 4 of 2026-09-29) ------------------------------------------------

/**
 * The quests of a new character's first enter world in its start map (Poeta 210010000 / Ishalgen 220010000), from their Java handlers:
 *
 * - CM_ENTER_WORLD: PlayerEnterWorldService.java:204 calls PlayerController.onLevelChange(0, 1), which runs QuestEngine.onLevelChanged and
 *   then updateNearbyQuests (PlayerController.java: onLevelChange). _1100KaliosCall.java:64-67 / _2100OrderoftheCaptain.java:62-65 call
 *   defaultOnLevelChangedEvent in their start map; the mission's minlevel_permitted is 3, a level-1 character is within its two levels
 *   (AbstractQuestHandler.java:995-998), so it is added LOCKED (:1026-1030, QuestService.addOrUpdateQuest: SM_QUEST_ACTION ADD) before
 *   SM_NEARBY_QUESTS, and SM_QUEST_LIST then holds it (PlayerEnterWorldService.java:238: every quest that is not COMPLETE). At level 3 the
 *   same hook sets it START (:1033-1034).
 * - CM_LEVEL_READY: QuestEngine.onEnterWorld (CM_LEVEL_READY.java:93) runs _1000Prologue.java:26-36 / _2000Prologue.java:26-36:
 *   QuestService.startQuest (SM_QUEST_ACTION ADD START, then SM_NEARBY_QUESTS: QuestService.java:441-442) and playQuestMovie(env, 1 / 2, true),
 *   SM_PLAY_MOVIE of a skippable CutSceneMovie with no target (AbstractQuestHandler.java:665-667).
 * - CM_PLAY_MOVIE_END: onMovieEndEvent (_1000Prologue.java:38-48) sets REWARD and QuestService.finishQuest (QuestService.java:77-118) pays
 *   reward group 0 of quest_data.xml's `<rewards exp="1"/>` (PlayerCommonData.addExp: SM_STATUPDATE_EXP, SM_SYSTEM_MESSAGE), then
 *   SM_QUEST_ACTION UPDATE COMPLETE and SM_NEARBY_QUESTS.
 */
struct Prologue {
	int32_t startMap = 0;
	/** 1000 / 2000: started at the first CM_LEVEL_READY, finished at the end of its movie */
	int32_t quest = 0;
	/** 1 / 2 (_1000Prologue.java:31, _2000Prologue.java:31) */
	int32_t movie = 0;
	/** 1100 / 2100: LOCKED at the first CM_ENTER_WORLD of a character below level 3 in the start map */
	int32_t mission = 0;
};

inline constexpr Prologue ELYOS_PROLOGUE{210010000, 1000, 1, 1100};
inline constexpr Prologue ASMODIAN_PROLOGUE{220010000, 2000, 2, 2100};
/** quest_data.xml 1000 and 2000: `<rewards exp="1"/>`, times gameserver.rates.xp.quest (config/main/rates.properties:59, 1.0 for membership 0) */
inline constexpr int64_t PROLOGUE_EXP = 1;
/**
 * SM_SYSTEM_MESSAGE.STR_GET_EXP2 (SM_SYSTEM_MESSAGE.java:16363-16365, "You have gained %num1 XP."): the movie's end has no target, so
 * QuestService.giveReward passes no npc name to PlayerCommonData.addExp, which then sends this variant (PlayerCommonData.java: addExp)
 */
inline constexpr int32_t PROLOGUE_EXP_MESSAGE = 1370002;

} // namespace aion::gameserver::scenario::decoders
