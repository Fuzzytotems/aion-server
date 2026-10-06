#pragma once

// The alliance packets of the M5g alliance gate (m5g-plan.md §10.5, §16.3 item 7), each decoded from the Java writeImpl field order (D9), the
// same way TeamDecoders.h does for the party packets.

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "decoders/TeamDecoders.h"

namespace aion::gameserver::scenario::decoders {

/** PlayerAllianceEvent.getId() (PlayerAllianceEvent.java:7-29); JOIN and MEMBER_GROUP_CHANGE share 5, the ENTER family shares 13 */
inline constexpr uint8_t ALLIANCE_EVENT_LEAVE = 0;
inline constexpr uint8_t ALLIANCE_EVENT_MOVEMENT = 1;
inline constexpr uint8_t ALLIANCE_EVENT_DISCONNECTED = 3;
inline constexpr uint8_t ALLIANCE_EVENT_JOIN = 5;
inline constexpr uint8_t ALLIANCE_EVENT_ENTER_OFFLINE = 7;
inline constexpr uint8_t ALLIANCE_EVENT_ENTER = 13;
inline constexpr uint8_t ALLIANCE_EVENT_UPDATE_EFFECTS = 65;

/** one alliance of SM_ALLIANCE_INFO's league block */
struct LeagueAllianceInfo {
	int32_t position = 0;
	int32_t allianceObjectId = 0;
	int32_t memberCount = 0;
	std::string captainName;
	int32_t captainWorldId = 0;
};

/** SM_ALLIANCE_INFO.java writeImpl; the league block is present only in a league */
struct AllianceInfo {
	int32_t groupSize = 0;
	int32_t allianceId = 0;
	int32_t leaderId = 0;
	int32_t mapId = 0;
	std::vector<int32_t> viceCaptains; // the non-zero of the four slots, in order
	std::vector<int32_t> lootWords;    // loot rule id, misc and the six quality words
	int32_t type = 0;
	int32_t subType = 0;
	int32_t leagueId = 0;
	int32_t messageId = 0;
	std::string message;
	int32_t leagueAlliances = 0; // 0 without a league block
	std::vector<int32_t> leagueLootWords; // the league block's loot rule id, misc and six quality words
	std::vector<LeagueAllianceInfo> league;
};
AllianceInfo decodeAllianceInfo(std::span<const uint8_t> body);

/** SM_ALLIANCE_MEMBER_INFO.java writeImpl */
struct AllianceMemberInfo {
	int32_t allianceGroupId = 0;
	int32_t objectId = 0;
	int32_t maxHp = 0, currentHp = 0, maxMp = 0, currentMp = 0, maxFp = 0, currentFp = 0;
	int32_t mapId = 0;
	int32_t instanceKey = 0;
	float x = 0, y = 0, z = 0;
	uint8_t classId = 0, genderId = 0, level = 0;
	uint8_t event = 0;
	uint8_t flyState = 0;
	std::optional<std::string> name;
	/** true for the id-5 body that carries the name only (MEMBER_GROUP_CHANGE), false for JOIN */
	bool groupChange = false;
	std::optional<uint8_t> slot;
	std::vector<GroupMemberEffect> effects;
};
AllianceMemberInfo decodeAllianceMemberInfo(std::span<const uint8_t> body);

/** SM_ALLIANCE_READY_CHECK.java writeImpl: writeD(playerObjectId), writeC(statusCode) */
struct AllianceReadyCheck {
	int32_t playerObjectId = 0;
	uint8_t statusCode = 0;
};
AllianceReadyCheck decodeAllianceReadyCheck(std::span<const uint8_t> body);

} // namespace aion::gameserver::scenario::decoders
