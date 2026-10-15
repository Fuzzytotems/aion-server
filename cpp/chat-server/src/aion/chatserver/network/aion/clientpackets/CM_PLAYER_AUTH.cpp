#include "aion/chatserver/network/aion/clientpackets/CM_PLAYER_AUTH.h"

#include <fmt/format.h>

#include "aion/chatserver/service/ChatService.h"
#include "aion/chatserver/utils/Utf16Le.h"
#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/StringUtils.h"

namespace aion::chatserver::network::aion::clientpackets {

namespace Utf16Le = utils::Utf16Le;

void CM_PLAYER_AUTH::readImpl() {
	identifierSeparator = Utf16Le::decode(readB(2)); // @
	readC(); // 0
	readD(); // 1
	int32_t gameNameLength = readH() * 2;
	readB(gameNameLength); // AION
	readD(); // 27
	readD(); // 1 or 3
	readD(); // 0
	playerId = readD();
	readD(); // 0
	readD(); // 0
	readD(); // 0
	int32_t length = readH() * 2;
	identifier = readB(length);
	int32_t accountNameLength = readH() * 2;
	accountName = Utf16Le::newString(readB(accountNameLength));
	int32_t tokenLength = readH();
	token = readB(tokenLength);
}

void CM_PLAYER_AUTH::runImpl() {
	std::u16string nameIdentifier = Utf16Le::decode(identifier); // Name@identifier
	size_t separator = nameIdentifier.rfind(identifierSeparator);
	if (separator == std::u16string::npos) // Java: substring(0, -1)
		throw commons::utils::IndexOutOfBoundsException(fmt::format("begin 0, end -1, length {}", nameIdentifier.size()));
	std::string charName = commons::utils::StringUtils::toUtf8(std::u16string_view(nameIdentifier).substr(0, separator));
	service::ChatService::getInstance().registerPlayerConnection(playerId, token, identifier, charName, accountName, clientChannelHandler);
}

} // namespace aion::chatserver::network::aion::clientpackets
