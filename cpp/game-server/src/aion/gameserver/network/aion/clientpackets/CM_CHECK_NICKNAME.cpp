#include "aion/gameserver/network/aion/clientpackets/CM_CHECK_NICKNAME.h"

#include <memory>
#include <optional>

#include "aion/gameserver/configs/main/GSConfig.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CREATE_CHARACTER.h"
#include "aion/gameserver/network/aion/serverpackets/SM_NICKNAME_CHECK_RESPONSE.h"
#include "aion/gameserver/services/NameRestrictionService.h"
#include "aion/gameserver/services/player/PlayerService.h"
#include "aion/gameserver/utils/Util.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_CHECK_NICKNAME::CM_CHECK_NICKNAME(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

void CM_CHECK_NICKNAME::readImpl() {
	nick = readS();
}

void CM_CHECK_NICKNAME::runImpl() {
	using serverpackets::SM_CREATE_CHARACTER;
	using serverpackets::SM_NICKNAME_CHECK_RESPONSE;
	using services::NameRestrictionService;
	const std::shared_ptr<AionConnection>& con = getConnection();
	nick = utils::Util::convertName(nick);
	if (services::player::PlayerService::isNameUsedOrReserved(std::nullopt, nick)) {
		if (configs::main::GSConfig::CHARACTER_CREATION_MODE.load() == 2)
			con->sendPacket(SM_NICKNAME_CHECK_RESPONSE(SM_CREATE_CHARACTER::RESPONSE_NAME_RESERVED));
		else
			con->sendPacket(SM_NICKNAME_CHECK_RESPONSE(SM_CREATE_CHARACTER::RESPONSE_NAME_ALREADY_USED));
	} else if (!NameRestrictionService::isValidName(nick)) {
		con->sendPacket(SM_NICKNAME_CHECK_RESPONSE(SM_CREATE_CHARACTER::RESPONSE_INVALID_NAME));
	} else if (NameRestrictionService::isForbidden(nick)) {
		con->sendPacket(SM_NICKNAME_CHECK_RESPONSE(SM_CREATE_CHARACTER::RESPONSE_FORBIDDEN_CHAR_NAME));
	} else {
		con->sendPacket(SM_NICKNAME_CHECK_RESPONSE(SM_CREATE_CHARACTER::RESPONSE_OK));
	}
}

AION_CLIENT_PACKET(CM_CHECK_NICKNAME);

} // namespace aion::gameserver::network::aion::clientpackets
