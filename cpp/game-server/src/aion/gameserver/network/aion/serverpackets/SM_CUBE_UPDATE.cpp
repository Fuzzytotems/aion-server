#include "aion/gameserver/network/aion/serverpackets/SM_CUBE_UPDATE.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/items/storage/StorageType.h"
#include "aion/gameserver/model/team/legion/Legion.h"
#include "aion/gameserver/model/team/legion/LegionWarehouse.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_CUBE_UPDATE SM_CUBE_UPDATE::stigmaSlots(int32_t slots) {
	return SM_CUBE_UPDATE(6, slots);
}

SM_CUBE_UPDATE SM_CUBE_UPDATE::cubeSize(model::items::storage::StorageType type, model::gameobjects::player::Player& player) {
	int32_t itemsCount = 0;
	int32_t npcExpands = 0;
	int32_t questExpands = 0;
	int32_t itemExpands = 0;
	switch (type) {
		case model::items::storage::StorageType::CUBE:
			itemsCount = player.getInventory().size();
			npcExpands = player.getNpcExpands();
			questExpands = player.getQuestExpands();
			itemExpands = player.getItemExpands();
			break;
		case model::items::storage::StorageType::REGULAR_WAREHOUSE:
			itemsCount = player.getWarehouse().size();
			npcExpands = player.getWhNpcExpands();
			questExpands = player.getWhBonusExpands();
			break;
		case model::items::storage::StorageType::LEGION_WAREHOUSE:
			itemsCount = player.getLegion()->getLegionWarehouse().size();
			npcExpands = player.getLegion()->getWarehouseExpansions();
			break;
		default:
			break;
	}
	return SM_CUBE_UPDATE(0, static_cast<int32_t>(type), itemsCount, npcExpands, questExpands, itemExpands); // Java: type.ordinal()
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
	writeC(action);
	writeC(actionValue);
	switch (action) {
		case 0:
			writeD(itemsCount);
			writeC(npcExpands); // cube size from npc (so max 5 for now)
			writeC(questExpands); // cube size from quest (so max 2 for now)
			writeC(itemExpands); // count of used items (tickets)
			break;
	}
}

} // namespace aion::gameserver::network::aion::serverpackets
