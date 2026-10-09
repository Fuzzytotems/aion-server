#include "aion/gameserver/network/aion/clientpackets/CM_HOUSE_SCRIPT.h"

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerScripts.h"
#include "aion/gameserver/model/house/House.h"
#include "aion/gameserver/model/templates/housing/HouseAddress.h"
#include "aion/gameserver/network/aion/serverpackets/SM_HOUSE_SCRIPTS.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/fields/Array.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/audit/AuditLogger.h"
#include "aion/gameserver/network/aion/AionConnection.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

using model::gameobjects::player::Player;
using serverpackets::SM_HOUSE_SCRIPTS;


CM_HOUSE_SCRIPT::CM_HOUSE_SCRIPT(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_HOUSE_SCRIPT.java:36-47
void CM_HOUSE_SCRIPT::readImpl() {
	address = readD();
	scriptId = readUC();
	totalSize = readUH();
	if (totalSize > 0) {
		compressedSize = readD();
		if (compressedSize <= SM_HOUSE_SCRIPTS::MAX_COMPRESSED_SCRIPT_SIZE) {
			uncompressedSize = readD();
			scriptContent = readB(compressedSize);
		}
	}
}

// Java CM_HOUSE_SCRIPT.java:50-67
void CM_HOUSE_SCRIPT::runImpl() {
	const runtime::Ptr<Player> player = getConnection()->getActivePlayer();
	if (compressedSize > SM_HOUSE_SCRIPTS::MAX_COMPRESSED_SCRIPT_SIZE) {
		utils::PacketSendUtility::sendPacket(*player, serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_SCRIPT_OVERFLOW());
		return;
	}
	const runtime::Ptr<model::house::House> house = player->getActiveHouse();
	if (!house || house->getAddress()->getId() != address) {
		utils::audit::AuditLogger::log(*player, "tried to modify script of house they don't own (address " + std::to_string(address) + ")");
		return;
	}
	const runtime::Ptr<model::gameobjects::player::PlayerScripts> scripts = house->getPlayerScripts();
	if (totalSize == 0) {
		scripts->remove(scriptId);
	} else {
		// Java byte[]: the read bytes as a runtime array
		runtime::Ref<runtime::Array<int8_t>> content = runtime::Array<int8_t>::make(static_cast<int32_t>(scriptContent.size()));
		for (int32_t i = 0; i < static_cast<int32_t>(scriptContent.size()); i++)
			(*content)[i] = static_cast<int8_t>(scriptContent[static_cast<size_t>(i)]);
		scripts->set(scriptId, content, uncompressedSize);
	}
	// Java PacketSendUtility.broadcastPacket(VisibleObject, packet): the players who know the sender, not the sender (the M5h gate's C18)
	utils::PacketSendUtility::broadcastPacket(static_cast<model::gameobjects::VisibleObject&>(*player), SM_HOUSE_SCRIPTS(address, scripts->get(scriptId)));
}

AION_CLIENT_PACKET(CM_HOUSE_SCRIPT);

} // namespace aion::gameserver::network::aion::clientpackets
