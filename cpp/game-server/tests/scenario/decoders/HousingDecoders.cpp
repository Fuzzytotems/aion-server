#include "decoders/HousingDecoders.h"

#include <string>

namespace aion::gameserver::scenario::decoders {

namespace {

/** AbstractPlayerInfoPacket.CHARNAME_MAX_LENGTH and AbstractHouseInfoPacket.SIGN_NOTICE_MAX_LENGTH, the fixed lengths of writeCommonInfo's strings */
constexpr size_t CHARNAME_MAX_LENGTH = 25;
constexpr size_t SIGN_NOTICE_MAX_LENGTH = 64;

void readCommonInfo(BodyReader& reader, HouseInfo& info) {
	reader.expectD(0, "writeD(0)");                       // AbstractHouseInfoPacket.writeCommonInfo, writeD(0)
	info.address = reader.D();                            // writeD(house.getAddress().getId())
	info.ownerId = reader.D();                            // writeD(house.getOwnerId())
	info.buildingTypeId = reader.D();                     // writeD(house.getBuilding().getType().getId())
	reader.expectC(1, "writeC(1)");                       // writeC(1)
	info.buildingId = reader.D();                         // writeD(house.getBuilding().getId())
	info.ownerStates = reader.C();                        // writeC(house.getHouseOwnerStates())
	info.doorState = reader.C();                          // writeC(house.getDoorState().getId())
	info.ownerName = reader.S(CHARNAME_MAX_LENGTH);       // writeS(house.getOwnerName(), CHARNAME_MAX_LENGTH)
	info.legionId = reader.D();                           // writeD(member == null ? 0 : legion id)
	info.showOwnerName = reader.C();                      // writeC(house.isShowOwnerName() ? 1 : 0)
	info.signNotice = reader.S(SIGN_NOTICE_MAX_LENGTH);   // writeS(house.getSignNotice(), SIGN_NOTICE_MAX_LENGTH)
	for (size_t i = 0; i < HOUSE_DECOR_SLOTS; i++)
		info.decorIds.push_back(reader.D()); // for each PartType, for each room: writeD(decorId == null ? 0 : decorId)
	reader.expectZeros(9, "writeD(0), writeD(0), writeC(0)");
	info.emblem = reader.B(6); // the emblem id, type and colours, or writeB(new byte[6])
}

/** AionServerPacket.writeDyeInfo: writeB(new byte[4]) or writeC(1) + the colour's three bytes */
void skipDyeInfo(BodyReader& reader) {
	reader.skip(4);
}

HouseObjectUsage readUsage(BodyReader& reader) {
	HouseObjectUsage usage;
	usage.useCount = reader.D();  // UseableItemObject.UseDataWriter.writeMe, writeD(use count)
	usage.checkType = reader.C(); // writeC(check type)
	return usage;
}

} // namespace

HouseAcquire decodeHouseAcquire(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_HOUSE_ACQUIRE");
	HouseAcquire acquire;
	acquire.playerId = reader.D(); // SM_HOUSE_ACQUIRE.java writeImpl, writeD(playerId)
	acquire.address = reader.D();  // writeD(address)
	acquire.acquire = reader.D();  // writeD(acquire ? 1 : 0)
	reader.expectFullyConsumed();
	return acquire;
}

HouseInfo decodeHouseRender(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_HOUSE_RENDER");
	HouseInfo info;
	readCommonInfo(reader, info); // SM_HOUSE_RENDER.java writeImpl
	reader.expectFullyConsumed();
	return info;
}

HouseInfo decodeHouseUpdate(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_HOUSE_UPDATE");
	HouseInfo info;
	reader.expectH(1, "writeH(1)"); // SM_HOUSE_UPDATE.java writeImpl
	reader.expectH(0, "writeH(0)");
	reader.expectH(1, "writeH(1)");
	readCommonInfo(reader, info);
	reader.expectFullyConsumed();
	return info;
}

HouseScripts decodeHouseScripts(std::span<const uint8_t> body, size_t paddingSize) {
	BodyReader reader(body, "SM_HOUSE_SCRIPTS");
	HouseScripts scripts;
	scripts.address = reader.D(); // SM_HOUSE_SCRIPTS.java writeImpl, writeD(houseAddress)
	const uint16_t count = reader.H(); // writeH(scripts.size())
	for (uint16_t i = 0; i < count; i++) {
		HouseScript script;
		script.id = reader.C();            // writeC(script.id())
		const uint16_t following = reader.H(); // writeH(8 + content + padding) or writeH(0)
		if (following != 0) {
			script.hasData = true;
			const int32_t contentAndPadding = reader.D(); // writeD(scriptContent.length + SCRIPT_PADDING.length)
			script.uncompressedSize = reader.D();         // writeD(script.uncompressedSize())
			if (contentAndPadding < static_cast<int32_t>(paddingSize) || following != 8 + contentAndPadding)
				reader.fail("script sizes " + std::to_string(following) + "/" + std::to_string(contentAndPadding));
			script.compressed = reader.B(static_cast<size_t>(contentAndPadding) - paddingSize); // writeB(scriptContent)
			script.padding = reader.B(paddingSize);                                             // writeB(SCRIPT_PADDING)
		}
		scripts.scripts.push_back(std::move(script));
	}
	reader.expectFullyConsumed();
	return scripts;
}

HouseObjectInfo decodeHouseObject(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_HOUSE_OBJECT");
	HouseObjectInfo info;
	info.address = reader.D();  // SM_HOUSE_OBJECT.java writeImpl, writeD(house address)
	info.ownerId = reader.D();  // writeD(house owner)
	info.objectId = reader.D(); // writeD(houseObject.getObjectId())
	if (reader.D() != info.objectId) // writeD(houseObject.getObjectId()) again
		reader.fail("the second object id differs");
	info.templateId = reader.D();             // writeD(templateId)
	info.x = reader.F();                      // writeF(x)
	info.y = reader.F();                      // writeF(y)
	info.z = reader.F();                      // writeF(z)
	info.rotation = reader.H();               // writeH(rotation)
	info.cooldownSeconds = reader.D();        // writeD(remainingSeconds)
	info.secondsUntilExpiration = reader.D(); // writeD(secondsUntilExpiration)
	skipDyeInfo(reader);                      // writeDyeInfo(color)
	reader.expectD(0, "writeD(0)");           // writeD(0)
	info.typeId = reader.C();                 // writeC(typeId)
	if (info.typeId == HOUSE_OBJECT_TYPE_USE_ITEM)
		info.usage = readUsage(reader); // case 1: writeUsageData
	else if (info.typeId == HOUSE_OBJECT_TYPE_NPC)
		info.npcObjectId = reader.D(); // case 7: writeD(npcObj.getNpcObjectId())
	reader.expectFullyConsumed();
	return info;
}

HouseEdit decodeHouseEdit(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_HOUSE_EDIT");
	HouseEdit edit;
	edit.action = reader.C(); // SM_HOUSE_EDIT.java writeImpl, writeC(action)
	switch (edit.action) {
		case 3:                                       // add item
			edit.storeId = reader.C();                // writeC(storeId)
			edit.objectId = reader.D();               // writeD(itemObjectId)
			edit.templateId = reader.D();             // writeD(templateId)
			edit.secondsUntilExpiration = reader.D(); // writeD(obj == null ? 0 : secondsUntilExpiration)
			skipDyeInfo(reader);                      // writeDyeInfo
			reader.expectD(0, "writeD(0)");           // writeD(0)
			edit.typeId = reader.C();                 // writeC(typeId)
			if (reader.remaining() > 0) {             // obj instanceof UseableItemObject
				edit.userId = reader.D();             // writeD(player.getObjectId())
				edit.usage = readUsage(reader);       // writeUsageData
			}
			break;
		case 4:                         // remove from inventory
			edit.storeId = reader.C();  // writeC(storeId)
			edit.objectId = reader.D(); // writeD(itemObjectId)
			break;
		case 5:                                       // spawn or move
			edit.address = reader.D();                // writeD(house.getAddress().getId())
			edit.playerId = reader.D();               // writeD(player object id)
			edit.objectId = reader.D();               // writeD(itemObjectId)
			edit.templateId = reader.D();             // writeD(template id)
			edit.x = reader.F();                      // writeF(x)
			edit.y = reader.F();                      // writeF(y)
			edit.z = reader.F();                      // writeF(z)
			edit.rotation = reader.H();               // writeH(rotation)
			edit.cooldownSeconds = reader.D();        // writeD(remainingSeconds)
			edit.secondsUntilExpiration = reader.D(); // writeD(obj.secondsUntilExpiration())
			skipDyeInfo(reader);                      // writeDyeInfo
			reader.expectD(0, "writeD(0)");           // writeD(0)
			edit.typeId = reader.C();                 // writeC(typeId)
			if (reader.remaining() > 0)               // obj instanceof UseableItemObject
				edit.usage = readUsage(reader);
			break;
		case 7:                         // despawn
			edit.objectId = reader.D(); // writeD(itemObjectId)
			break;
		default: // writeC(action) only
			break;
	}
	reader.expectFullyConsumed();
	return edit;
}

int32_t decodeDeleteHouseObject(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_DELETE_HOUSE_OBJECT");
	const int32_t objectId = reader.D(); // SM_DELETE_HOUSE_OBJECT.java writeImpl, writeD(itemObjectId)
	reader.expectFullyConsumed();
	return objectId;
}

ObjectUseUpdate decodeObjectUseUpdate(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_OBJECT_USE_UPDATE");
	ObjectUseUpdate update;
	update.typeId = reader.C(); // SM_OBJECT_USE_UPDATE.java writeImpl, writeC(typeId)
	if (update.typeId != HOUSE_OBJECT_TYPE_USE_ITEM)
		reader.fail("type " + std::to_string(update.typeId) + " is not a UseableItemObject's");
	update.userId = reader.D();    // writeD(usingPlayerId)
	update.ownerId = reader.D();   // writeD(ownerPlayerId)
	update.objectId = reader.D();  // writeD(object.getObjectId())
	update.useCount = reader.D();  // writeD(useCount)
	update.checkType = reader.C(); // writeC(checkType)
	reader.expectFullyConsumed();
	return update;
}

LegionMemberList decodeLegionMemberList(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_LEGION_MEMBERLIST");
	LegionMemberList list;
	list.first = reader.C();  // SM_LEGION_MEMBERLIST.java writeImpl, writeC(isFirst ? 1 : 0)
	list.count = reader.Hs(); // writeH(isLast ? -size : size)
	const int32_t size = list.count < 0 ? -list.count : list.count;
	for (int32_t i = 0; i < size; i++) {
		LegionMemberEntry entry;
		entry.objectId = reader.D();       // writeLegionMember: writeD(getObjectId())
		entry.name = reader.S();           // writeS(getName())
		entry.classId = reader.C();        // writeC(class id)
		entry.level = reader.D();          // writeD(getLevel())
		entry.rank = reader.C();           // writeC(rank id)
		entry.worldId = reader.D();        // writeD(getWorldId())
		entry.online = reader.C();         // writeC(isOnline() ? 1 : 0)
		entry.selfIntro = reader.S();      // writeS(getSelfIntro())
		entry.nickname = reader.S();       // writeS(getNickname())
		entry.lastOnline = reader.D();     // writeD(online ? 0 : last online)
		entry.houseAddress = reader.D();   // writeD(house == null ? 0 : address)
		entry.houseDoorState = reader.D(); // writeD(house == null ? 0 : door state)
		entry.serverId = reader.D();       // writeD(NetworkConfig.GAMESERVER_ID)
		list.members.push_back(std::move(entry));
	}
	reader.expectFullyConsumed();
	return list;
}

LegionUpdateMember decodeLegionUpdateMember(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_LEGION_UPDATE_MEMBER");
	LegionUpdateMember update;
	update.objectId = reader.D();   // SM_LEGION_UPDATE_MEMBER.java writeImpl, writeD(getObjectId())
	update.rank = reader.C();       // writeC(rank id)
	update.classId = reader.C();    // writeC(class id)
	update.level = reader.C();      // writeC(getLevel())
	update.worldId = reader.D();    // writeD(getWorldId())
	update.online = reader.C();     // writeC(isOnline() ? 1 : 0)
	update.lastOnline = reader.D(); // writeD(online ? 0 : last online)
	update.serverId = reader.D();   // writeD(NetworkConfig.GAMESERVER_ID)
	update.messageId = reader.D();  // writeD(msgId)
	update.text = reader.S();       // writeS(text)
	reader.expectFullyConsumed();
	return update;
}

} // namespace aion::gameserver::scenario::decoders
