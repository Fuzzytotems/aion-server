#include "aion/gameserver/network/aion/serverpackets/SM_FORTRESS_STATUS.h"

#include <vector>

#include "aion/gameserver/model/siege/FortressLocation.h"
#include "aion/gameserver/model/siege/Influence.h"
#include "aion/gameserver/model/siege/SiegeRace.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/services/SiegeService.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_FORTRESS_STATUS::SM_FORTRESS_STATUS() : AionServerPacket(opcodeOf<SM_FORTRESS_STATUS>) {
}

// Java SM_FORTRESS_STATUS.java:14-36 (M5i I-6, under P5-12a's lease)
void SM_FORTRESS_STATUS::writeImpl(AionConnection* con) {
	static_cast<void>(con);
	writeC(1);
	model::siege::Influence& inf = model::siege::Influence::getInstance();
	writeD(services::SiegeService::getInstance().getSecondsUntilNextFortressState());
	writeF(inf.getElyosInfluenceRate());
	writeF(inf.getAsmodianInfluenceRate());
	writeF(inf.getBalaurInfluenceRate());
	const std::vector<int32_t> worldIds = inf.getInfluenceRelevantWorldIds();
	writeH(static_cast<int32_t>(worldIds.size()));
	for (int32_t worldId : worldIds) {
		writeD(worldId);
		writeF(static_cast<float>(inf.getInfluence(worldId, model::siege::SiegeRace::ELYOS))); // Java: writeF(int)
		writeF(static_cast<float>(inf.getInfluence(worldId, model::siege::SiegeRace::ASMODIANS)));
		writeF(static_cast<float>(inf.getInfluence(worldId, model::siege::SiegeRace::BALAUR)));
	}
	const std::vector<runtime::Ptr<model::siege::FortressLocation>> fortresses = services::SiegeService::getInstance().getFortresses().values();
	writeH(static_cast<int32_t>(fortresses.size()));
	for (const runtime::Ptr<model::siege::FortressLocation>& fortress : fortresses) {
		writeD(fortress->getLocationId());
		writeC(fortress->getNextState());
	}
}

} // namespace aion::gameserver::network::aion::serverpackets
