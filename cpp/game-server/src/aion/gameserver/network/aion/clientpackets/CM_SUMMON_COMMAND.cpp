#include "aion/gameserver/network/aion/clientpackets/CM_SUMMON_COMMAND.h"

#include <optional>

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/Summon.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/summons/SummonMode.h"
#include "aion/gameserver/model/summons/SummonModeInfo.h"
#include "aion/gameserver/model/summons/UnsummonType.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/summons/SummonsService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_SUMMON_COMMAND::CM_SUMMON_COMMAND(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_SUMMON_COMMAND.java:26-31
void CM_SUMMON_COMMAND::readImpl() {
	mode = readUC();
	readD(); // 0
	readD(); // 0
	targetObjId = readD();
}

// Java CM_SUMMON_COMMAND.java:33-41
void CM_SUMMON_COMMAND::runImpl() {
	const runtime::Ptr<model::gameobjects::player::Player> activePlayer = getConnection()->getActivePlayer();
	const runtime::Ptr<model::gameobjects::Summon> summon = activePlayer->getSummon();
	const std::optional<model::summons::SummonMode> summonMode = model::summons::getSummonModeById(mode);
	if (summon && summonMode) {
		services::summons::SummonsService::doMode(*summonMode, *summon, targetObjId, model::summons::UnsummonType::COMMAND);
	}
}

AION_CLIENT_PACKET(CM_SUMMON_COMMAND);

} // namespace aion::gameserver::network::aion::clientpackets
