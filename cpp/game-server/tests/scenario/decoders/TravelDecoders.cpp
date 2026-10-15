#include "decoders/TravelDecoders.h"

#include <string>

namespace aion::gameserver::scenario::decoders {

TeleportMap decodeTeleportMap(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_TELEPORT_MAP");
	TeleportMap map;
	map.targetObjectId = reader.D(); // SM_TELEPORT_MAP.java writeImpl, writeD(targetObjId)
	map.teleportId = reader.H();     // writeH(teleportId)
	reader.expectFullyConsumed();
	return map;
}

TeleportLoc decodeTeleportLoc(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_TELEPORT_LOC");
	TeleportLoc loc;
	loc.animation = reader.C();       // SM_TELEPORT_LOC.java writeImpl, writeC(portAnimation)
	loc.mapId = reader.D();           // writeD(mapId)
	loc.mapOrInstanceId = reader.D(); // writeD(isInstance ? instanceId : mapId)
	loc.x = reader.F();               // writeF(x)
	loc.y = reader.F();               // writeF(y)
	loc.z = reader.F();               // writeF(z)
	loc.heading = reader.C();         // writeC(heading)
	reader.expectFullyConsumed();
	return loc;
}

ChannelInfo decodeChannelInfo(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_CHANNEL_INFO");
	ChannelInfo info;
	info.currentChannel = reader.D(); // SM_CHANNEL_INFO.java writeImpl, writeD(currentChannel)
	info.instanceCount = reader.D();  // writeD(instanceCount)
	reader.expectFullyConsumed();
	return info;
}

BindPointTeleport decodeBindPointTeleport(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_BIND_POINT_TELEPORT");
	BindPointTeleport teleport;
	teleport.action = reader.C();   // SM_BIND_POINT_TELEPORT.java writeImpl, writeC(action)
	teleport.playerId = reader.D(); // writeD(playerId)
	switch (teleport.action) {      // switch (action)
		case 1:
			teleport.locId = reader.D(); // case 1: writeD(locId)
			break;
		case 3:
			teleport.locId = reader.D();    // case 3: writeD(locId)
			teleport.cooldown = reader.D(); // writeD(cooldown)
			break;
		default:
			break;
	}
	reader.expectFullyConsumed();
	return teleport;
}

BindPointInfo decodeBindPointInfo(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_BIND_POINT_INFO");
	BindPointInfo info;
	info.type = reader.C();                        // SM_BIND_POINT_INFO.java writeImpl, writeC(bindPointType)
	reader.expectC(0x01, "SM_BIND_POINT_INFO's second byte (writeC(0x01))"); // writeC(0x01)
	info.mapId = reader.D();                       // writeD(mapId)
	info.x = reader.F();                           // writeF(x)
	info.y = reader.F();                           // writeF(y)
	info.z = reader.F();                           // writeF(z)
	info.kiskObjectId = reader.D();                // writeD(kiskObjId)
	if (info.type != 0 && info.type != 4)
		reader.fail("the bind point type is " + std::to_string(info.type) + ", but the two constructors write 0 or 4");
	reader.expectFullyConsumed();
	return info;
}

std::optional<InstanceCooldownEntry> InstanceInfo::entry(int32_t id) const {
	if (players.empty())
		return std::nullopt;
	for (const InstanceCooldownEntry& candidate : players.front().entries)
		if (candidate.cooltimeId == id)
			return candidate;
	return std::nullopt;
}

InstanceInfo decodeInstanceInfo(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_INSTANCE_INFO");
	InstanceInfo info;
	info.updateType = reader.C();                               // SM_INSTANCE_INFO.java writeImpl, writeC(updateType)
	info.cooltimeId = reader.D();                               // writeD(instanceIds.length == 1 ? the cooltime id : 0)
	reader.expectC(0x00, "SM_INSTANCE_INFO's unk1 (writeC(0x00))"); // writeC(0x00)
	const uint16_t playerCount = reader.H();                    // writeH(players.size())
	for (uint16_t p = 0; p < playerCount; p++) {
		InstanceInfoPlayer player;
		player.objectId = reader.D();               // writeD(player.getObjectId())
		const uint16_t instanceCount = reader.H(); // writeH(instanceIds.length)
		for (uint16_t i = 0; i < instanceCount; i++) {
			InstanceCooldownEntry entry;
			entry.cooltimeId = reader.D();                                     // writeD(cooltime.getId())
			reader.expectD(0, "SM_INSTANCE_INFO's int after the cooltime id"); // writeD(0x00)
			entry.reuseSeconds = reader.D();                                   // writeD(remaining seconds)
			entry.maxCount = reader.D();                                       // writeD(cooltime.getMaxCount())
			entry.entryOffset = reader.D();                                    // writeD(-enterCount)
			entry.show = reader.C();                                           // writeC(hide flag)
			if (entry.show > 1)
				reader.fail("the show flag is " + std::to_string(entry.show) + ", but Java writes 0 or 1");
			player.entries.push_back(entry);
		}
		player.name = reader.S(); // writeS(player.getName())
		info.players.push_back(std::move(player));
	}
	reader.expectFullyConsumed();
	return info;
}

InstanceCountInfo decodeInstanceCountInfo(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_INSTANCE_COUNT_INFO");
	InstanceCountInfo info;
	info.mapId = reader.D();      // SM_INSTANCE_COUNT_INFO.java writeImpl, writeD(mapId)
	info.instanceId = reader.D(); // writeD(instanceId)
	reader.expectD(1, "SM_INSTANCE_COUNT_INFO's last int (writeD(1), \"1 solo\")"); // writeD(1)
	reader.expectFullyConsumed();
	return info;
}

UseObject decodeUseObject(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_USE_OBJECT");
	UseObject use;
	use.playerObjectId = reader.D(); // SM_USE_OBJECT.java writeImpl, writeD(playerObjId)
	use.targetObjectId = reader.D(); // writeD(targetObjId)
	use.time = reader.D();           // writeD(time)
	use.actionType = reader.C();     // writeC(actionType)
	reader.expectFullyConsumed();
	return use;
}

Delete decodeDelete(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_DELETE");
	Delete deleted;
	deleted.objectId = reader.D();  // SM_DELETE.java writeImpl, writeD(objectId)
	deleted.animation = reader.C(); // writeC(animationId)
	reader.expectFullyConsumed();
	return deleted;
}

SystemMessage decodeSystemMessage(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_SYSTEM_MESSAGE");
	SystemMessage message;
	message.chatType = reader.C();                                           // SM_SYSTEM_MESSAGE.java writeImpl, writeC(chatType)
	reader.expectC(0x00, "SM_SYSTEM_MESSAGE's second byte (writeC(0x00))"); // writeC(0x00)
	message.senderObjectId = reader.D();                                     // writeD(senderObjId)
	message.messageId = reader.D();                                          // writeD(msgId)
	const uint8_t params = reader.C();                                       // writeC(params.length)
	for (uint8_t i = 0; i < params; i++)
		message.params.push_back(reader.S()); // writeS(param == null ? null : param.toString())
	const uint8_t special = reader.C();       // writeC(specialParams.length)
	for (uint8_t i = 0; i < special; i++)
		message.specialParams.push_back(reader.S()); // writeS(param)
	reader.expectFullyConsumed();
	return message;
}

} // namespace aion::gameserver::scenario::decoders
