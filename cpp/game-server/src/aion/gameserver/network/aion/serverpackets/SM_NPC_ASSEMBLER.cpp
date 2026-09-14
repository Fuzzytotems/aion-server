#include "aion/gameserver/network/aion/serverpackets/SM_NPC_ASSEMBLER.h"

#include "aion/gameserver/model/assemblednpc/AssembledNpc.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_NPC_ASSEMBLER::SM_NPC_ASSEMBLER(runtime::Ptr<model::assemblednpc::AssembledNpc> assembledNpcValue)
	: AionServerPacket(opcodeOf<SM_NPC_ASSEMBLER>) {
	AION_UNPORTED();
}

SM_NPC_ASSEMBLER::~SM_NPC_ASSEMBLER() = default;

void SM_NPC_ASSEMBLER::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
