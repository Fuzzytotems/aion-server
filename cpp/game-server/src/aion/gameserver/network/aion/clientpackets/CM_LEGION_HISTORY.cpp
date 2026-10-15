#include "aion/gameserver/network/aion/clientpackets/CM_LEGION_HISTORY.h"

#include <string>

#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/legion/Legion.h"
#include "aion/gameserver/model/team/legion/LegionMember.h"
#include "aion/gameserver/network/aion/serverpackets/SM_LEGION_HISTORY.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/network/aion/AionConnection.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

using model::gameobjects::player::Player;
using model::team::legion::LegionHistoryAction_Type;


CM_LEGION_HISTORY::CM_LEGION_HISTORY(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_LEGION_HISTORY.java:27-31
void CM_LEGION_HISTORY::readImpl() {
	page = readD();
	// Java: Type.values()[readUC()]
	const int32_t ordinal = readUC();
	constexpr int32_t TYPES = static_cast<int32_t>(xml::EnumTraits<LegionHistoryAction_Type>::names.size());
	if (ordinal >= TYPES)
		throw runtime::ArrayIndexOutOfBoundsException("Index " + std::to_string(ordinal) + " out of bounds for length " + std::to_string(TYPES));
	type = static_cast<LegionHistoryAction_Type>(ordinal);
}

// Java CM_LEGION_HISTORY.java:34-41
void CM_LEGION_HISTORY::runImpl() {
	const runtime::Ptr<Player> player = getConnection()->getActivePlayer();
	if (!player->getLegion())
		return;
	if (type == LegionHistoryAction_Type::REWARD && !player->getLegionMember()->isBrigadeGeneral())
		return;
	sendPacket(serverpackets::SM_LEGION_HISTORY(player->getLegion()->getHistory(type), page, type));
}

AION_CLIENT_PACKET(CM_LEGION_HISTORY);

} // namespace aion::gameserver::network::aion::clientpackets
