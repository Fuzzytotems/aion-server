#include "aion/gameserver/network/aion/clientpackets/CM_SECURITY_TOKEN.h"

#include <cstdint>
#include <span>
#include <string>

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SECURITY_TOKEN.h"
#include "aion/gameserver/services/player/SecurityTokenService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_SECURITY_TOKEN::CM_SECURITY_TOKEN(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

void CM_SECURITY_TOKEN::readImpl() {
}

void CM_SECURITY_TOKEN::runImpl() {
	runtime::Ptr<model::account::Account> account = getConnection()->getAccount();
	if (!account)
		return;
	if (account->getSecurityToken().empty())
		services::player::SecurityTokenService::generateToken(*account);
	// Java: getBytes() in the platform charset (UTF-8); the token is Base64 text
	const std::string token = account->getSecurityToken();
	sendPacket(serverpackets::SM_SECURITY_TOKEN(std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(token.data()), token.size())));
}

AION_CLIENT_PACKET(CM_SECURITY_TOKEN);

} // namespace aion::gameserver::network::aion::clientpackets
