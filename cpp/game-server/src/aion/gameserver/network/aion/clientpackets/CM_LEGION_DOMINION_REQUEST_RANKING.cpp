#include "aion/gameserver/network/aion/clientpackets/CM_LEGION_DOMINION_REQUEST_RANKING.h"

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/legionDominion/LegionDominionLocation.h"
#include "aion/gameserver/model/team/legion/Legion.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/serverpackets/SM_LEGION_DOMINION_RANK.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/LegionDominionService.h"

namespace aion::gameserver::network::aion::clientpackets {

CM_LEGION_DOMINION_REQUEST_RANKING::CM_LEGION_DOMINION_REQUEST_RANKING(int32_t opcode, const StateSet& validStates)
	: AionClientPacket(opcode, validStates) {
}

// Java CM_LEGION_DOMINION_REQUEST_RANKING.java:24-27
void CM_LEGION_DOMINION_REQUEST_RANKING::readImpl() {
	stonespearId = readD();
}

// Java CM_LEGION_DOMINION_REQUEST_RANKING.java:29-36
void CM_LEGION_DOMINION_REQUEST_RANKING::runImpl() {
	if (stonespearId >= 1 && stonespearId <= 6) { // idk sometimes it sends different bytes! TODO
		runtime::Ptr<model::legionDominion::LegionDominionLocation> location = services::LegionDominionService::getInstance().getLegionDominionLoc(stonespearId);
		runtime::Ptr<model::team::legion::Legion> legion = getConnection()->getActivePlayer()->getLegion();
		if (!location) // Java: new SM_LEGION_DOMINION_RANK(null, legion) throws in its constructor
			throw runtime::NullPointerException("LegionDominionLocation " + std::to_string(stonespearId));
		sendPacket(serverpackets::SM_LEGION_DOMINION_RANK(*location, legion));
	}
}

AION_CLIENT_PACKET(CM_LEGION_DOMINION_REQUEST_RANKING);

} // namespace aion::gameserver::network::aion::clientpackets
