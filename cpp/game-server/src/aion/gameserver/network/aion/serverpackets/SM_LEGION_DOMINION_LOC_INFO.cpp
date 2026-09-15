#include "aion/gameserver/network/aion/serverpackets/SM_LEGION_DOMINION_LOC_INFO.h"

#include <string>
#include <vector>

#include "aion/gameserver/model/legionDominion/LegionDominionLocation.h"
#include "aion/gameserver/model/team/legion/Legion.h"
#include "aion/gameserver/model/team/legion/LegionEmblem.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/serverpackets/detail/PacketLookups.h"
#include "aion/gameserver/network/aion/serverpackets/detail/PacketSupport.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_LEGION_DOMINION_LOC_INFO::SM_LEGION_DOMINION_LOC_INFO()
	: AionServerPacket(opcodeOf<SM_LEGION_DOMINION_LOC_INFO>) {
}

void SM_LEGION_DOMINION_LOC_INFO::writeImpl(AionConnection* con) {
	writeH(static_cast<int32_t>(detail::getLegionDominions().size()));
	for (const runtime::Ptr<model::legionDominion::LegionDominionLocation>& loc : detail::getLegionDominions()) {
		runtime::Ptr<model::team::legion::Legion> legion = loc->getLegionId() == 0 ? nullptr : detail::getLegion(loc->getLegionId());
		runtime::Ref<model::team::legion::LegionEmblem> emblem =
			legion == nullptr ? model::team::legion::LegionEmblem::create() : runtime::Ref<model::team::legion::LegionEmblem>(legion->getLegionEmblem());
		writeD(loc->getLocationId());
		writeD(loc->getLegionId());
		writeC(emblem->getEmblemId());
		writeC(detail::legionEmblemTypeValue(emblem->getEmblemType()));
		writeC(emblem->getColor_a());
		writeC(emblem->getColor_r());
		writeC(emblem->getColor_g());
		writeC(emblem->getColor_b());
		writeS(legion == nullptr ? std::string() : legion->getName());
	}
}

} // namespace aion::gameserver::network::aion::serverpackets
