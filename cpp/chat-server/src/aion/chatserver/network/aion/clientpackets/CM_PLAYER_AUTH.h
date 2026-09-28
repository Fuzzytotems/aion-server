#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "aion/chatserver/network/aion/AbstractClientPacket.h"

namespace aion::chatserver::network::aion::clientpackets {

/**
 * The client's login with the token the game server gave it (SM_CHAT_INIT): the player id, "name@identifier", the account name and the token.
 * Answered with SM_PLAYER_AUTH_RESPONSE if everything matches what the game server registered (see ChatService::registerPlayerConnection).
 * <p>
 * Java: com.aionemu.chatserver.network.aion.clientpackets.CM_PLAYER_AUTH
 *
 * @author ATracer
 */
class CM_PLAYER_AUTH : public AbstractClientPacket {
public:
	CM_PLAYER_AUTH(common::netty::ChannelBuffer channelBuffer, std::shared_ptr<netty::handler::ClientChannelHandler> clientChannelHandler, int8_t opCode) noexcept
		: AbstractClientPacket(std::move(channelBuffer), std::move(clientChannelHandler), opCode) {}

protected:
	void readImpl() override;

	/**
	 * The character name is the identifier up to the last occurrence of the separator (the first two bytes of the packet, "@").
	 * @throws commons::utils::IndexOutOfBoundsException if the identifier does not contain the separator (Java: StringIndexOutOfBoundsException)
	 */
	void runImpl() override;

private:
	int32_t playerId = 0;
	std::vector<uint8_t> token;
	std::vector<uint8_t> identifier;
	/** Java: new String(readB(2), StandardCharsets.UTF_16LE), kept in UTF-16 for the lastIndexOf in runImpl */
	std::u16string identifierSeparator;
	std::string accountName;
};

} // namespace aion::chatserver::network::aion::clientpackets
