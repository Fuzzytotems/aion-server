#include "aion/gameserver/network/aion/serverpackets/SM_INFLUENCE_RATIO.h"

#include <vector>

#include "aion/gameserver/model/siege/FortressLocation.h"
#include "aion/gameserver/model/siege/Influence.h"
#include "aion/gameserver/model/siege/SiegeRace.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/services/SiegeService.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_INFLUENCE_RATIO::SM_INFLUENCE_RATIO() : AionServerPacket(opcodeOf<SM_INFLUENCE_RATIO>) {
}

// Java SM_INFLUENCE_RATIO.java:14-29 (M5i I-6, under P5-12a's lease)
void SM_INFLUENCE_RATIO::writeImpl(AionConnection* con) {
	static_cast<void>(con);
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
}

} // namespace aion::gameserver::network::aion::serverpackets
