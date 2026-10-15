#include "aion/gameserver/network/aion/serverpackets/SM_NPC_ASSEMBLER.h"

#include "aion/gameserver/model/assemblednpc/AssembledNpc.h"
#include "aion/gameserver/model/assemblednpc/AssembledNpcPart.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/serverpackets/detail/PacketSupport.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_NPC_ASSEMBLER::SM_NPC_ASSEMBLER(runtime::Ptr<model::assemblednpc::AssembledNpc> assembledNpcValue)
	: AionServerPacket(opcodeOf<SM_NPC_ASSEMBLER>), assembledNpc(assembledNpcValue) {
	if (assembledNpcValue != nullptr) {
		routeId = assembledNpcValue->getRouteId();
		timeOnMap = assembledNpcValue->getTimeOnMap();
	}
}

SM_NPC_ASSEMBLER::~SM_NPC_ASSEMBLER() = default;

void SM_NPC_ASSEMBLER::writeImpl(AionConnection* con) {
	if (assembledNpc != nullptr) {
		const auto parts = assembledNpc->getAssembledParts().snapshot();
		writeD(static_cast<int32_t>(parts.size())); // size
		for (const runtime::Ptr<model::assemblednpc::AssembledNpcPart>& npc : parts) {
			writeD(routeId); // routeId
			writeD(detail::unbox(npc->getObject(), "AssembledNpcPart.getObject()")); // objectId
			writeD(npc->getNpcId()); // npc Id
			writeD(npc->getStaticId()); // static Id
			writeQ(timeOnMap); // time
		}
	} else {
		writeD(0);
	}
}

} // namespace aion::gameserver::network::aion::serverpackets
