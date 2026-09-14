#include "aion/gameserver/network/aion/serverpackets/SM_MOTION.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/player/motion/Motion.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_MOTION::SM_MOTION(const std::vector<runtime::Ptr<model::gameobjects::player::motion::Motion>>& motionsValue)
	: AionServerPacket(opcodeOf<SM_MOTION>), action(1), motions(motionsValue.begin(), motionsValue.end()) {
}

SM_MOTION::SM_MOTION(int16_t motionIdValue, int32_t remainingTimeValue)
	: AionServerPacket(opcodeOf<SM_MOTION>), action(2), motionId(motionIdValue), remainingTime(remainingTimeValue) {
}

SM_MOTION::SM_MOTION(int16_t motionIdValue, int8_t typeValue)
	: AionServerPacket(opcodeOf<SM_MOTION>), action(5), motionId(motionIdValue), type(typeValue) {
}

SM_MOTION::SM_MOTION(int16_t motionIdValue)
	: AionServerPacket(opcodeOf<SM_MOTION>), action(6), motionId(motionIdValue) {
}

SM_MOTION::SM_MOTION(int32_t playerIdValue,
	const std::unordered_map<int32_t, runtime::Ptr<model::gameobjects::player::motion::Motion>>& activeMotionsValue)
	: AionServerPacket(opcodeOf<SM_MOTION>), action(7), playerId(playerIdValue), activeMotions(activeMotionsValue.begin(), activeMotionsValue.end()) {
}

SM_MOTION::~SM_MOTION() = default;

void SM_MOTION::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
