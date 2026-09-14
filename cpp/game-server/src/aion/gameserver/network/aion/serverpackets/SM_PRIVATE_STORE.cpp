#include "aion/gameserver/network/aion/serverpackets/SM_PRIVATE_STORE.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PrivateStore.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_PRIVATE_STORE::SM_PRIVATE_STORE(runtime::Ptr<model::gameobjects::player::PrivateStore> storeValue, model::gameobjects::player::Player& playerValue)
	: AionServerPacket(opcodeOf<SM_PRIVATE_STORE>), player(playerValue), store(storeValue) {
}

SM_PRIVATE_STORE::~SM_PRIVATE_STORE() = default;

void SM_PRIVATE_STORE::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
