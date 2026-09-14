#include "aion/gameserver/network/aion/serverpackets/SM_CONQUEROR_PROTECTOR.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_CONQUEROR_PROTECTOR::SM_CONQUEROR_PROTECTOR(int32_t typeValue, int32_t buffLvlValue, int32_t cooldownValue)
	: AionServerPacket(opcodeOf<SM_CONQUEROR_PROTECTOR>), type(typeValue), buffLvl(buffLvlValue), cooldown(cooldownValue) {
}

SM_CONQUEROR_PROTECTOR::SM_CONQUEROR_PROTECTOR(int32_t typeValue, int32_t buffLvlValue)
	: AionServerPacket(opcodeOf<SM_CONQUEROR_PROTECTOR>), type(typeValue), buffLvl(buffLvlValue) {
}

SM_CONQUEROR_PROTECTOR::SM_CONQUEROR_PROTECTOR(int32_t typeValue, model::gameobjects::player::Player& playerValue)
	: AionServerPacket(opcodeOf<SM_CONQUEROR_PROTECTOR>), type(typeValue), player(playerValue) {
}

SM_CONQUEROR_PROTECTOR::SM_CONQUEROR_PROTECTOR(const std::vector<runtime::Ptr<model::gameobjects::player::Player>>& intrudersValue, bool displayCd)
	: AionServerPacket(opcodeOf<SM_CONQUEROR_PROTECTOR>), type(displayCd ? 5 : 4), intruders(intrudersValue.begin(), intrudersValue.end()) {
}

SM_CONQUEROR_PROTECTOR::~SM_CONQUEROR_PROTECTOR() = default;

void SM_CONQUEROR_PROTECTOR::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
