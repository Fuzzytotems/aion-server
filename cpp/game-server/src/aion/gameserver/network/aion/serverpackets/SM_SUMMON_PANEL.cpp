#include "aion/gameserver/network/aion/serverpackets/SM_SUMMON_PANEL.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/Summon.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_SUMMON_PANEL::SM_SUMMON_PANEL(model::gameobjects::Summon& summonValue)
	: AionServerPacket(opcodeOf<SM_SUMMON_PANEL>), summon(summonValue) {
}

SM_SUMMON_PANEL::~SM_SUMMON_PANEL() = default;

void SM_SUMMON_PANEL::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
