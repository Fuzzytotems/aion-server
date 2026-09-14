#include "aion/gameserver/network/aion/serverpackets/SM_RENAME.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_RENAME::SM_RENAME(model::gameobjects::player::Player& player, std::string_view oldNameValue)
	: AionServerPacket(opcodeOf<SM_RENAME>) {
	AION_UNPORTED();
}

SM_RENAME::SM_RENAME(model::team::legion::Legion& legion, std::string_view oldNameValue)
	: AionServerPacket(opcodeOf<SM_RENAME>) {
	AION_UNPORTED();
}

SM_RENAME::SM_RENAME(bool isLegionValue, int32_t playerOrLegionIdValue, std::string_view oldNameValue, std::string_view newNameValue)
	: AionServerPacket(opcodeOf<SM_RENAME>), isLegion(isLegionValue), playerOrLegionId(playerOrLegionIdValue), oldName(oldNameValue),
	  newName(newNameValue) {
}

void SM_RENAME::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
