#include "aion/gameserver/network/aion/clientpackets/CM_UI_SETTINGS.h"

#include <memory>
#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerSettings.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/runtime/fields/Array.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.network.aion.clientpackets.CM_UI_SETTINGS");

CM_UI_SETTINGS::CM_UI_SETTINGS(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

void CM_UI_SETTINGS::readImpl() {
	settingsType = readC();
	readH();
	size = readUH();
	data = readB(getRemainingBytes());
}

void CM_UI_SETTINGS::runImpl() {
	runtime::Ptr<model::gameobjects::player::Player> player = getConnection()->getActivePlayer();
	// Java byte[] data: a new array object stored in the settings
	auto bytes = [this] {
		runtime::Ref<runtime::Array<int8_t>> array = runtime::Array<int8_t>::make(static_cast<int32_t>(data.size()));
		for (size_t i = 0; i < data.size(); i++)
			(*array)[static_cast<int32_t>(i)] = static_cast<int8_t>(data[i]);
		return array;
	};
	switch (settingsType) {
		case 0:
			player->getPlayerSettings()->setUiSettings(bytes());
			break;
		case 1:
			player->getPlayerSettings()->setShortcuts(bytes());
			break;
		case 2:
			player->getPlayerSettings()->setHouseBuddies(bytes());
			break;
		default:
			log.warn((player ? player->toString() : std::string("null")) + " sent unknown type of player settings: " + std::to_string(settingsType));
	}
}

AION_CLIENT_PACKET(CM_UI_SETTINGS);

} // namespace aion::gameserver::network::aion::clientpackets
