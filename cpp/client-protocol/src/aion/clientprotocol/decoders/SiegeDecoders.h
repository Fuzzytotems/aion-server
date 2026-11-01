#pragma once

// The siege and world-event packets the M5i gates read (m5i-plan.md G-03): the siege locations at login and their state changes, the fortress
// teleport flag, the fortress status and the influence bar, the shields, the artifacts, the rift announcements (all five actions), the
// 4.7.5 location info and the assembled npcs.
//
// **m5a-plan.md D9:** every layout is written from the Java `writeImpl` under game-server/src/com/aionemu/gameserver/network/aion/serverpackets/
// (SM_SIEGE_LOCATION_INFO.java, SM_SIEGE_LOCATION_STATE.java, SM_FORTRESS_INFO.java, SM_FORTRESS_STATUS.java, SM_INFLUENCE_RATIO.java,
// SM_SHIELD_EFFECT.java, SM_ABYSS_ARTIFACT_INFO3.java, SM_RIFT_ANNOUNCE.java, SM_AFTER_SIEGE_LOCINFO_475.java, SM_NPC_ASSEMBLER.java). Nothing
// here includes, calls or mirrors a C++ serverpackets header.
//
// Every decode function consumes the body exactly and throws DecodeError otherwise; the Java constants that carry no data are verified.

#include <cstdint>
#include <span>
#include <vector>

#include "aion/clientprotocol/decoders/PacketDecoders.h" // BodyReader, DecodeError

namespace aion::clientprotocol::decoders {

// ---- SM_SIEGE_LOCATION_INFO --------------------------------------------------------------------------------------------------------------

/** one location of SM_SIEGE_LOCATION_INFO (40 bytes): D(locId) D(legionId) C(emblemId) C(emblemType) H(0) C(a) C(r) C(g) C(b) C(raceId)
 * C(vulnerable ? 2 : 0) C(canTeleport ? 1 : 0) C(nextState) H(0) H(0) D(veille/masta timer) D(server id) D(0) D(occupiedCount) */
struct SiegeLocationEntry {
	int32_t locationId = 0;
	int32_t legionId = 0;
	uint8_t emblemId = 0, emblemType = 0, colorA = 0, colorR = 0, colorG = 0, colorB = 0;
	uint8_t raceId = 0;
	bool vulnerable = false;
	bool canTeleport = false;
	uint8_t nextState = 0;
	int32_t remainingSiegeSeconds = 0; // only 2111 and 3111 (Silentera's outposts), else 0
	int32_t serverId = 0;
	int32_t occupiedCount = 0;
};

/** SM_SIEGE_LOCATION_INFO.writeImpl: with sieges disabled C(0) H(0); else C(infoType: 0 all at login, 1 one location) H(count) and the entries
 * in the iteration order of the service's LinkedHashMap */
struct SiegeLocationInfo {
	uint8_t infoType = 0;
	std::vector<SiegeLocationEntry> locations;
};

SiegeLocationInfo decodeSiegeLocationInfo(std::span<const uint8_t> body);

// ---- SM_SIEGE_LOCATION_STATE / SM_FORTRESS_INFO ------------------------------------------------------------------------------------------

/** SM_SIEGE_LOCATION_STATE.writeImpl: D(locationId) C(state: 1 vulnerable, 0 not) */
struct SiegeLocationState {
	int32_t locationId = 0;
	uint8_t state = 0;
};

SiegeLocationState decodeSiegeLocationState(std::span<const uint8_t> body);

/** SM_FORTRESS_INFO.writeImpl: D(locationId) C(teleportStatus ? 1 : 0) */
struct FortressInfo {
	int32_t locationId = 0;
	bool teleportStatus = false;
};

FortressInfo decodeFortressInfo(std::span<const uint8_t> body);

// ---- SM_INFLUENCE_RATIO / SM_FORTRESS_STATUS ---------------------------------------------------------------------------------------------

/** one world of the influence bar: D(worldId) F(elyos) F(asmodians) F(balaur) */
struct WorldInfluence {
	int32_t worldId = 0;
	float elyos = 0, asmodians = 0, balaur = 0;
};

/** SM_INFLUENCE_RATIO.writeImpl: D(seconds until the next fortress state) F(elyos) F(asmodians) F(balaur) H(count) and the worlds */
struct InfluenceRatio {
	int32_t secondsUntilNextFortressState = 0;
	float elyos = 0, asmodians = 0, balaur = 0;
	std::vector<WorldInfluence> worlds;
};

InfluenceRatio decodeInfluenceRatio(std::span<const uint8_t> body);

/** SM_FORTRESS_STATUS.writeImpl: C(1), the SM_INFLUENCE_RATIO body, then H(fortress count) and per fortress D(locationId) C(nextState) */
struct FortressStatus {
	InfluenceRatio influence;
	struct Fortress {
		int32_t locationId = 0;
		uint8_t nextState = 0;
	};
	std::vector<Fortress> fortresses;
};

FortressStatus decodeFortressStatus(std::span<const uint8_t> body);

// ---- SM_SHIELD_EFFECT / SM_ABYSS_ARTIFACT_INFO3 ------------------------------------------------------------------------------------------

/** SM_SHIELD_EFFECT.writeImpl: H(count), per location D(locationId) C(underShield ? 1 : 0) */
struct ShieldEffect {
	struct Location {
		int32_t locationId = 0;
		bool underShield = false;
	};
	std::vector<Location> locations;
};

ShieldEffect decodeShieldEffect(std::span<const uint8_t> body);

/** SM_ABYSS_ARTIFACT_INFO3.writeImpl: H(count), per artifact D(locationId * 10 + 1) C(status) D(0); the decoder gives back the location id */
struct ArtifactInfo3 {
	struct Artifact {
		int32_t locationId = 0;
		uint8_t status = 0;
	};
	std::vector<Artifact> artifacts;
};

ArtifactInfo3 decodeArtifactInfo3(std::span<const uint8_t> body);

// ---- SM_RIFT_ANNOUNCE --------------------------------------------------------------------------------------------------------------------

/**
 * SM_RIFT_ANNOUNCE.writeImpl: H(following byte length) C(actionId), then by action - 0 announce: D per rift value; 1 Silentera: D(gelkmaros)
 * D(inggison); 2 master: D(objectId) D(maxEntries) D(remainTime) D(minLevel) D(maxLevel) F(x) F(y) F(z) C(riftType) C(isMaster); 3 slave:
 * D(objectId) D(usedEntries) D(remainTime) C(riftType) C(0); 4 despawn: D(objectId). The length must match the bytes that follow it.
 */
struct RiftAnnounce {
	uint8_t actionId = 0;
	std::vector<int32_t> riftValues;      // action 0
	int32_t gelkmaros = 0, inggison = 0;  // action 1
	int32_t objectId = 0;                 // actions 2, 3, 4
	int32_t maxEntries = 0, usedEntries = 0, remainTime = 0, minLevel = 0, maxLevel = 0;
	float x = 0, y = 0, z = 0;
	uint8_t riftType = 0; // 0 plain, 1 vortex, 4 volatile, 5 invasion
	bool master = false;
};

RiftAnnounce decodeRiftAnnounce(std::span<const uint8_t> body);

// ---- SM_AFTER_SIEGE_LOCINFO_475 / SM_NPC_ASSEMBLER ---------------------------------------------------------------------------------------

/** SM_AFTER_SIEGE_LOCINFO_475.writeImpl: H(0) C(0); the decoder verifies the three zero bytes */
void decodeAfterSiegeLocInfo475(std::span<const uint8_t> body);

/** SM_NPC_ASSEMBLER.writeImpl: D(count), per part D(routeId) D(objectId) D(npcId) D(staticId) Q(timeOnMap); D(0) without an assembled npc */
struct NpcAssembler {
	struct Part {
		int32_t routeId = 0, objectId = 0, npcId = 0, staticId = 0;
		int64_t timeOnMap = 0;
	};
	std::vector<Part> parts;
};

NpcAssembler decodeNpcAssembler(std::span<const uint8_t> body);

} // namespace aion::clientprotocol::decoders
