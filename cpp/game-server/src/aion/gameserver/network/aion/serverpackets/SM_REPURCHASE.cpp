#include "aion/gameserver/network/aion/serverpackets/SM_REPURCHASE.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_REPURCHASE::SM_REPURCHASE(model::gameobjects::player::Player& playerValue, int32_t npcId)
	: AionServerPacket(opcodeOf<SM_REPURCHASE>), player(playerValue), targetObjectId(npcId) {
	AION_UNPORTED();
}

SM_REPURCHASE::~SM_REPURCHASE() = default;

void SM_REPURCHASE::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
