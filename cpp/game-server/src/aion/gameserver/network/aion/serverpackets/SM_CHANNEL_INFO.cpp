#include "aion/gameserver/network/aion/serverpackets/SM_CHANNEL_INFO.h"

#include "aion/gameserver/configs/main/WorldConfig.h"
#include "aion/gameserver/model/templates/world/WorldMapTemplate.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_CHANNEL_INFO::SM_CHANNEL_INFO(runtime::Ptr<world::WorldPosition> position) : AionServerPacket(opcodeOf<SM_CHANNEL_INFO>) {
	if (!position || !position->isSpawned()) {
		instanceCount = 1;
		currentChannel = 1;
	} else {
		const model::templates::world::WorldMapTemplate* worldMapTemplate = position->getWorldMapInstance()->getTemplate();
		if (position->getWorldMapInstance()->isBeginnerInstance()) {
			instanceCount = worldMapTemplate->getBeginnerTwinCount();
			if (configs::main::WorldConfig::WORLD_EMULATE_FASTTRACK.load())
				instanceCount += worldMapTemplate->getTwinCount();
			currentChannel = position->getInstanceId() - 1;
		} else {
			instanceCount = worldMapTemplate->getTwinCount();
			if (configs::main::WorldConfig::WORLD_EMULATE_FASTTRACK.load())
				instanceCount += worldMapTemplate->getBeginnerTwinCount();
			currentChannel = position->getInstanceId() - 1;
		}
	}
}

void SM_CHANNEL_INFO::writeImpl(AionConnection* con) {
	writeD(currentChannel);
	writeD(instanceCount);
}

} // namespace aion::gameserver::network::aion::serverpackets
