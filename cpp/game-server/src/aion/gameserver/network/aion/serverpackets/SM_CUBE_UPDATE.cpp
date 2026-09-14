#include "aion/gameserver/network/aion/serverpackets/SM_CUBE_UPDATE.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_CUBE_UPDATE SM_CUBE_UPDATE::stigmaSlots(int32_t slots) {
	return SM_CUBE_UPDATE(6, slots);
}

SM_CUBE_UPDATE SM_CUBE_UPDATE::cubeSize(model::items::storage::StorageType type, model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

SM_CUBE_UPDATE::SM_CUBE_UPDATE(int32_t actionArg, int32_t actionValueValue, int32_t itemsCountValue, int32_t npcExpandsValue,
	int32_t questExpandsValue, int32_t itemExpandsValue)
	: SM_CUBE_UPDATE(actionArg, actionValueValue) {
	this->itemsCount = itemsCountValue;
	this->npcExpands = npcExpandsValue;
	this->questExpands = questExpandsValue;
	this->itemExpands = itemExpandsValue;
}

SM_CUBE_UPDATE::SM_CUBE_UPDATE(int32_t actionArg, int32_t actionValueValue)
	: AionServerPacket(opcodeOf<SM_CUBE_UPDATE>), action(actionArg), actionValue(actionValueValue) {
}

void SM_CUBE_UPDATE::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
