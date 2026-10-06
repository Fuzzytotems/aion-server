#pragma once

// The travel and instance packets the M5f gate reads (m5f-plan.md §2.1-§2.5, G-02; the §10.3 rows T1-T20): the teleporter's map, the teleport
// animation, the channel, the hotspot cast, the bind point, the instance list, the instance count, the use bar, the object deletion with its
// animation and the system message with its parameters. SM_PLAYER_SPAWN, SM_PLAYER_INFO (with its flight fields: PlayerInfoOptions) and
// SM_NPC_INFO are M5a's (PacketDecoders.h), SM_EMOTION M5b-1's (CombatDecoders.h), SM_DIALOG_WINDOW and SM_QUESTION_WINDOW M5c's
// (EconomyDecoders.h), SM_ACTION_ANIMATION M5e's (ProgressionDecoders.h), SM_CASTSPELL M5b-2's (SkillDecoders.h).
//
// **m5a-plan.md D9:** every layout is written from the Java `writeImpl` under game-server/src/com/aionemu/gameserver/network/aion/serverpackets/
// (SM_TELEPORT_MAP.java, SM_TELEPORT_LOC.java, SM_CHANNEL_INFO.java, SM_BIND_POINT_TELEPORT.java, SM_BIND_POINT_INFO.java, SM_INSTANCE_INFO.java,
// SM_INSTANCE_COUNT_INFO.java, SM_USE_OBJECT.java, SM_DELETE.java, SM_SYSTEM_MESSAGE.java). Nothing here includes, calls or mirrors a C++
// serverpackets header.
//
// Every decode function consumes the body exactly and throws DecodeError otherwise; the Java constants that carry no data are verified.

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "decoders/PacketDecoders.h" // BodyReader, DecodeError

namespace aion::gameserver::scenario::decoders {

// ---- SM_TELEPORT_MAP ---------------------------------------------------------------------------------------------------------------------

/** SM_TELEPORT_MAP.writeImpl: writeD(targetObjId), writeH(teleportId) */
struct TeleportMap {
	int32_t targetObjectId = 0;
	uint16_t teleportId = 0;
};

TeleportMap decodeTeleportMap(std::span<const uint8_t> body);

// ---- SM_TELEPORT_LOC ---------------------------------------------------------------------------------------------------------------------

/** TeleportAnimation ids (TeleportAnimation.java): NONE 0, FADE_OUT_BEAM 1, JUMP_IN 3, JUMP_IN_STATUE 4 (the four the gate meets) */
constexpr uint8_t TELEPORT_ANIMATION_NONE = 0;
constexpr uint8_t TELEPORT_ANIMATION_FADE_OUT_BEAM = 1;
constexpr uint8_t TELEPORT_ANIMATION_JUMP_IN = 3;
constexpr uint8_t TELEPORT_ANIMATION_JUMP_IN_STATUE = 4;

/**
 * SM_TELEPORT_LOC.writeImpl: writeC(animation.getId()), writeD(mapId), writeD(isInstance ? instanceId : mapId), writeF(x), writeF(y), writeF(z),
 * writeC(heading)
 */
struct TeleportLoc {
	uint8_t animation = 0;
	int32_t mapId = 0;
	/** the instance id on an instance map, else the map id again */
	int32_t mapOrInstanceId = 0;
	float x = 0, y = 0, z = 0;
	uint8_t heading = 0;
};

TeleportLoc decodeTeleportLoc(std::span<const uint8_t> body);

// ---- SM_CHANNEL_INFO ---------------------------------------------------------------------------------------------------------------------

/** SM_CHANNEL_INFO.writeImpl: writeD(currentChannel), writeD(instanceCount); (1, 1) for a position that is not spawned (the constructor) */
struct ChannelInfo {
	int32_t currentChannel = 0;
	int32_t instanceCount = 0;
};

ChannelInfo decodeChannelInfo(std::span<const uint8_t> body);

// ---- SM_BIND_POINT_TELEPORT --------------------------------------------------------------------------------------------------------------

/** SM_BIND_POINT_TELEPORT.writeImpl: writeC(action), writeD(playerId); action 1: writeD(locId); action 3: writeD(locId), writeD(cooldown) */
struct BindPointTeleport {
	uint8_t action = 0;
	int32_t playerId = 0;
	/** actions 1 and 3 only */
	int32_t locId = 0;
	/** action 3 only: the cooldown in seconds */
	int32_t cooldown = 0;
};

BindPointTeleport decodeBindPointTeleport(std::span<const uint8_t> body);

// ---- SM_BIND_POINT_INFO ------------------------------------------------------------------------------------------------------------------

/** SM_BIND_POINT_INFO.writeImpl: writeC(bindPointType) (0 obelisk, 4 kisk), writeC(0x01), writeD(mapId), writeF x/y/z, writeD(kiskObjId) */
struct BindPointInfo {
	uint8_t type = 0;
	int32_t mapId = 0;
	float x = 0, y = 0, z = 0;
	int32_t kiskObjectId = 0;
};

BindPointInfo decodeBindPointInfo(std::span<const uint8_t> body);

// ---- SM_INSTANCE_INFO --------------------------------------------------------------------------------------------------------------------

/** one instance of one player's block (SM_INSTANCE_INFO.java's inner loop) */
struct InstanceCooldownEntry {
	int32_t cooltimeId = 0;
	/** writeD(cooldown == null ? 0 : (int) (reuseTime - now) / 1000): the seconds left, not an absolute time */
	int32_t reuseSeconds = 0;
	int32_t maxCount = 0;
	/** writeD(cooldown == null ? 0 : -enterCount) */
	int32_t entryOffset = 0;
	/** writeC(cooltime.getRace() == opposite race ? 0 : 1) */
	uint8_t show = 0;
};

struct InstanceInfoPlayer {
	int32_t objectId = 0;
	std::vector<InstanceCooldownEntry> entries;
	std::string name;
};

/**
 * SM_INSTANCE_INFO.writeImpl: writeC(updateType), writeD(the cooltime id when exactly one instance is updated, else 0), writeC(0), writeH(players);
 * per player writeD(objectId), writeH(instances), per instance writeD(id), writeD(0), writeD(reuse s), writeD(max), writeD(offset), writeC(show);
 * then writeS(name)
 */
struct InstanceInfo {
	uint8_t updateType = 0;
	int32_t cooltimeId = 0;
	std::vector<InstanceInfoPlayer> players;

	/** the entry with that cooltime id in the first player's block, if any */
	std::optional<InstanceCooldownEntry> entry(int32_t cooltimeId) const;
};

InstanceInfo decodeInstanceInfo(std::span<const uint8_t> body);

// ---- SM_INSTANCE_COUNT_INFO --------------------------------------------------------------------------------------------------------------

/** SM_INSTANCE_COUNT_INFO.writeImpl: writeD(mapId), writeD(instanceId), writeD(1) */
struct InstanceCountInfo {
	int32_t mapId = 0;
	int32_t instanceId = 0;
};

InstanceCountInfo decodeInstanceCountInfo(std::span<const uint8_t> body);

// ---- SM_USE_OBJECT -----------------------------------------------------------------------------------------------------------------------

/** ActionItemNpcAI's startBarAnimation 1 and cancelBarAnimation 2 (ActionItemNpcAI.java:28-29) */
constexpr uint8_t USE_OBJECT_START_BAR = 1;
constexpr uint8_t USE_OBJECT_CANCEL_BAR = 2;

/** SM_USE_OBJECT.writeImpl: writeD(playerObjId), writeD(targetObjId), writeD(time), writeC(actionType) */
struct UseObject {
	int32_t playerObjectId = 0;
	int32_t targetObjectId = 0;
	int32_t time = 0;
	uint8_t actionType = 0;
};

UseObject decodeUseObject(std::span<const uint8_t> body);

// ---- SM_DELETE ---------------------------------------------------------------------------------------------------------------------------

/** ObjectDeleteAnimation ids (ObjectDeleteAnimation.java): NONE 0, FADE_OUT 1, (beam) 2, JUMP_IN 11 */
constexpr uint8_t DELETE_ANIMATION_NONE = 0;
constexpr uint8_t DELETE_ANIMATION_FADE_OUT = 1;
constexpr uint8_t DELETE_ANIMATION_JUMP_IN = 11;

/** SM_DELETE.writeImpl: writeD(objectId), writeC(animationId) */
struct Delete {
	int32_t objectId = 0;
	uint8_t animation = 0;
};

Delete decodeDelete(std::span<const uint8_t> body);

// ---- SM_SYSTEM_MESSAGE -------------------------------------------------------------------------------------------------------------------

/**
 * SM_SYSTEM_MESSAGE.writeImpl: writeC(chatType), writeC(0), writeD(senderObjId), writeD(msgId), writeC(params), per param writeS(String),
 * writeC(specialParams), per special param writeS
 */
struct SystemMessage {
	uint8_t chatType = 0;
	int32_t senderObjectId = 0;
	int32_t messageId = 0;
	std::vector<std::string> params;
	std::vector<std::string> specialParams;
};

SystemMessage decodeSystemMessage(std::span<const uint8_t> body);

} // namespace aion::gameserver::scenario::decoders
