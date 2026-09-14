#include "aion/gameserver/network/aion/serverpackets/SM_INSTANCE_INFO.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_INSTANCE_INFO::SM_INSTANCE_INFO(int8_t updateTypeValue, model::gameobjects::player::Player& player, std::initializer_list<int32_t> instanceId)
	: SM_INSTANCE_INFO(updateTypeValue, std::vector<runtime::Ptr<model::gameobjects::player::Player>>{player}, instanceId) {
}

SM_INSTANCE_INFO::SM_INSTANCE_INFO(int8_t updateTypeValue, const std::vector<runtime::Ptr<model::gameobjects::player::Player>>& playersValue,
	std::initializer_list<int32_t> instanceId)
	: AionServerPacket(opcodeOf<SM_INSTANCE_INFO>) {
	AION_UNPORTED();
}

SM_INSTANCE_INFO::~SM_INSTANCE_INFO() = default;

void SM_INSTANCE_INFO::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
