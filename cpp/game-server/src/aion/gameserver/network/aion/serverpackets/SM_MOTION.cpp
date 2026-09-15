#include "aion/gameserver/network/aion/serverpackets/SM_MOTION.h"

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
	writeC(action);
	switch (action) {
		case 1:
			writeH(static_cast<int32_t>(motions.size()));
			for (const runtime::Ref<model::gameobjects::player::motion::Motion>& motion : motions) {
				writeH(motion->getId());
				writeD(motion->secondsUntilExpiration());
				writeC(motion->isActive() ? 1 : 0);
			}
			break;
		case 2: // Add motion
			writeH(motionId);
			writeD(remainingTime);
			break;
		case 5: // Set motion
			writeH(motionId);
			writeC(type);
			break;
		case 6: // remove
			writeH(motionId);
			break;
		case 7: // Player motions
			writeD(playerId);
			for (int32_t i = 1; i < 6; i++) {
				auto motion = activeMotions.find(i);
				if (motion == activeMotions.end() || motion->second == nullptr)
					writeH(0);
				else
					writeH(motion->second->getId());
			}
	}
}

} // namespace aion::gameserver::network::aion::serverpackets
