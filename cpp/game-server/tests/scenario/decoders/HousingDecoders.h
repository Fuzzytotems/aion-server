#pragma once

// The studio packets the M5h gate reads (m5h-plan.md §2.7, G-03; the §10.3 rows Y13-Y19): the ownership, the house's appearance, the house
// scripts, the placed objects and the decoration mode's edits, the object use update, and the two legion packets that carry a member's map and
// house (the member list's entries and the member update).
//
// **m5a-plan.md D9:** every layout is written from the Java `writeImpl` under game-server/src/com/aionemu/gameserver/network/aion/serverpackets/
// (SM_HOUSE_ACQUIRE.java, AbstractHouseInfoPacket.java with SM_HOUSE_RENDER.java and SM_HOUSE_UPDATE.java, SM_HOUSE_SCRIPTS.java,
// SM_HOUSE_OBJECT.java, SM_HOUSE_EDIT.java, SM_DELETE_HOUSE_OBJECT.java, SM_OBJECT_USE_UPDATE.java, SM_LEGION_MEMBERLIST.java,
// SM_LEGION_UPDATE_MEMBER.java) and the helpers they call (AionServerPacket.writeS(text, fixedLength) and writeDyeInfo,
// UseableItemObject.UseDataWriter.writeMe). Nothing here includes, calls or mirrors a C++ serverpackets header.
//
// Every decode function consumes the body exactly and throws DecodeError otherwise; the Java constants that carry no data are verified.

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "decoders/PacketDecoders.h" // BodyReader, DecodeError

namespace aion::gameserver::scenario::decoders {

/** HousingUseableItem.getTypeId (HousingUseableItem.java): the type byte after which the usage data follows */
constexpr uint8_t HOUSE_OBJECT_TYPE_USE_ITEM = 1;
/** HousingNpc.getTypeId: the type byte after which the npc object id follows (SM_HOUSE_OBJECT.java, case 7) */
constexpr uint8_t HOUSE_OBJECT_TYPE_NPC = 7;
/** the decor id slots of AbstractHouseInfoPacket.writeCommonInfo: PartType's rooms summed (PartType.java: 6 parts of 1, 6 + 6 rooms, ADDON 1) */
constexpr size_t HOUSE_DECOR_SLOTS = 19;

// ---- SM_HOUSE_ACQUIRE --------------------------------------------------------------------------------------------------------------------

/** SM_HOUSE_ACQUIRE.writeImpl: writeD(playerId), writeD(address), writeD(acquire ? 1 : 0) */
struct HouseAcquire {
	int32_t playerId = 0;
	int32_t address = 0;
	int32_t acquire = 0;
};

HouseAcquire decodeHouseAcquire(std::span<const uint8_t> body);

// ---- SM_HOUSE_RENDER / SM_HOUSE_UPDATE ---------------------------------------------------------------------------------------------------

/** AbstractHouseInfoPacket.writeCommonInfo */
struct HouseInfo {
	int32_t address = 0;
	int32_t ownerId = 0;
	int32_t buildingTypeId = 0;
	int32_t buildingId = 0;
	uint8_t ownerStates = 0;
	uint8_t doorState = 0;
	std::string ownerName;
	int32_t legionId = 0;
	uint8_t showOwnerName = 0;
	std::string signNotice;
	/** PartType by PartType, room by room (HOUSE_DECOR_SLOTS) */
	std::vector<int32_t> decorIds;
	std::vector<uint8_t> emblem; // 6 bytes
};

/** SM_HOUSE_RENDER.writeImpl: writeCommonInfo() */
HouseInfo decodeHouseRender(std::span<const uint8_t> body);
/** SM_HOUSE_UPDATE.writeImpl: writeH(1), writeH(0), writeH(1), writeCommonInfo() */
HouseInfo decodeHouseUpdate(std::span<const uint8_t> body);

// ---- SM_HOUSE_SCRIPTS --------------------------------------------------------------------------------------------------------------------

/** One script of SM_HOUSE_SCRIPTS: writeC(id), then writeH(0) for an empty slot, else the sizes, the compressed bytes and SCRIPT_PADDING */
struct HouseScript {
	uint8_t id = 0;
	bool hasData = false;
	int32_t uncompressedSize = 0;
	std::vector<uint8_t> compressed;
	std::vector<uint8_t> padding;
};

/** SM_HOUSE_SCRIPTS.writeImpl: writeD(houseAddress), writeH(scripts.size()), the scripts */
struct HouseScripts {
	int32_t address = 0;
	std::vector<HouseScript> scripts;
};

/** @param paddingSize SCRIPT_PADDING's length (the oracle's), the bytes after the compressed data of a script with data */
HouseScripts decodeHouseScripts(std::span<const uint8_t> body, size_t paddingSize);

// ---- SM_HOUSE_OBJECT ---------------------------------------------------------------------------------------------------------------------

/** UseableItemObject.UseDataWriter.writeMe: writeD(use count), writeC(check type) */
struct HouseObjectUsage {
	int32_t useCount = 0;
	uint8_t checkType = 0;
};

/** SM_HOUSE_OBJECT.writeImpl */
struct HouseObjectInfo {
	int32_t address = 0;
	int32_t ownerId = 0;
	int32_t objectId = 0;
	int32_t templateId = 0;
	float x = 0, y = 0, z = 0;
	uint16_t rotation = 0;
	int32_t cooldownSeconds = 0;
	int32_t secondsUntilExpiration = 0;
	uint8_t typeId = 0;
	std::optional<HouseObjectUsage> usage;
	std::optional<int32_t> npcObjectId;
};

HouseObjectInfo decodeHouseObject(std::span<const uint8_t> body);

// ---- SM_HOUSE_EDIT -----------------------------------------------------------------------------------------------------------------------

/** SM_HOUSE_EDIT.writeImpl: writeC(action), then by action: 3 the added item, 4 the removed one, 5 the spawned object, 7 the despawned one */
struct HouseEdit {
	uint8_t action = 0;
	uint8_t storeId = 0;
	int32_t objectId = 0;
	int32_t templateId = 0;
	int32_t secondsUntilExpiration = 0;
	uint8_t typeId = 0;
	/** action 3 of a UseableItemObject: writeD(player.getObjectId()) before the usage data */
	std::optional<int32_t> userId;
	std::optional<HouseObjectUsage> usage;
	// action 5
	int32_t address = 0;
	int32_t playerId = 0;
	float x = 0, y = 0, z = 0;
	uint16_t rotation = 0;
	int32_t cooldownSeconds = 0;
};

HouseEdit decodeHouseEdit(std::span<const uint8_t> body);

// ---- SM_DELETE_HOUSE_OBJECT --------------------------------------------------------------------------------------------------------------

/** SM_DELETE_HOUSE_OBJECT.writeImpl: writeD(itemObjectId) */
int32_t decodeDeleteHouseObject(std::span<const uint8_t> body);

// ---- SM_OBJECT_USE_UPDATE ----------------------------------------------------------------------------------------------------------------

/** SM_OBJECT_USE_UPDATE.writeImpl for a UseableItemObject: writeC(typeId), writeD(user), writeD(owner), writeD(object), writeD(useCount), writeC(checkType) */
struct ObjectUseUpdate {
	uint8_t typeId = 0;
	int32_t userId = 0;
	int32_t ownerId = 0;
	int32_t objectId = 0;
	int32_t useCount = 0;
	uint8_t checkType = 0;
};

/** decodes the UseableItemObject arm (type byte 1); another type fails */
ObjectUseUpdate decodeObjectUseUpdate(std::span<const uint8_t> body);

// ---- SM_LEGION_MEMBERLIST / SM_LEGION_UPDATE_MEMBER --------------------------------------------------------------------------------------

/** SM_LEGION_MEMBERLIST.writeLegionMember */
struct LegionMemberEntry {
	int32_t objectId = 0;
	std::string name;
	uint8_t classId = 0;
	int32_t level = 0;
	uint8_t rank = 0;
	int32_t worldId = 0;
	uint8_t online = 0;
	std::string selfIntro;
	std::string nickname;
	int32_t lastOnline = 0;
	int32_t houseAddress = 0;
	int32_t houseDoorState = 0;
	int32_t serverId = 0;
};

/** SM_LEGION_MEMBERLIST.writeImpl: writeC(isFirst ? 1 : 0), writeH(isLast ? -size : size), the members */
struct LegionMemberList {
	uint8_t first = 0;
	int16_t count = 0;
	std::vector<LegionMemberEntry> members;
};

LegionMemberList decodeLegionMemberList(std::span<const uint8_t> body);

/** SM_LEGION_UPDATE_MEMBER.writeImpl */
struct LegionUpdateMember {
	int32_t objectId = 0;
	uint8_t rank = 0;
	uint8_t classId = 0;
	uint8_t level = 0;
	int32_t worldId = 0;
	uint8_t online = 0;
	int32_t lastOnline = 0;
	int32_t serverId = 0;
	int32_t messageId = 0;
	std::string text;
};

LegionUpdateMember decodeLegionUpdateMember(std::span<const uint8_t> body);

} // namespace aion::gameserver::scenario::decoders
