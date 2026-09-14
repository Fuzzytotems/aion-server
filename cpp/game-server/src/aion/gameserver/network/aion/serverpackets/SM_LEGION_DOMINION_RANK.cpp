#include "aion/gameserver/network/aion/serverpackets/SM_LEGION_DOMINION_RANK.h"

#include "aion/gameserver/model/legionDominion/LegionDominionLocation.h"
#include "aion/gameserver/model/legionDominion/LegionDominionParticipantInfo.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_LEGION_DOMINION_RANK::SM_LEGION_DOMINION_RANK(model::legionDominion::LegionDominionLocation& locValue,
	runtime::Ptr<model::team::legion::Legion> legion)
	: AionServerPacket(opcodeOf<SM_LEGION_DOMINION_RANK>), loc(locValue) {
	AION_UNPORTED();
}

SM_LEGION_DOMINION_RANK::~SM_LEGION_DOMINION_RANK() = default;

void SM_LEGION_DOMINION_RANK::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
