#include "aion/gameserver/network/aion/serverpackets/SM_RENAME.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/legion/Legion.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_RENAME::SM_RENAME(model::gameobjects::player::Player& player, std::string_view oldNameValue)
	: AionServerPacket(opcodeOf<SM_RENAME>) {
	isLegion = false;
	playerOrLegionId = player.getObjectId();
	oldName = oldNameValue;
	newName = player.getName();
}

SM_RENAME::SM_RENAME(model::team::legion::Legion& legion, std::string_view oldNameValue)
	: AionServerPacket(opcodeOf<SM_RENAME>) {
	isLegion = true;
	playerOrLegionId = legion.getObjectId();
	oldName = oldNameValue;
	newName = legion.getName();
}

SM_RENAME::SM_RENAME(bool isLegionValue, int32_t playerOrLegionIdValue, std::string_view oldNameValue, std::string_view newNameValue)
	: AionServerPacket(opcodeOf<SM_RENAME>), isLegion(isLegionValue), playerOrLegionId(playerOrLegionIdValue), oldName(oldNameValue),
	  newName(newNameValue) {
}

void SM_RENAME::writeImpl(AionConnection* con) {
	writeD(isLegion ? 1 : 0);
	writeD(0); // error code 3: name in use, 4: invalid name, 6: legion name in use, 7: invalid legion name, 8: legion holds keep, 9: legion disbanding
	writeD(playerOrLegionId);
	writeS(oldName);
	writeS(newName);
}

} // namespace aion::gameserver::network::aion::serverpackets
