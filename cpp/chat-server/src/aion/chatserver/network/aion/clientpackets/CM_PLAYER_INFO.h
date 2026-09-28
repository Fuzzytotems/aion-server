#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "aion/chatserver/network/aion/AbstractClientPacket.h"

namespace aion::chatserver::network::aion::clientpackets {

/**
 * Client sends this after authentication and after each teleport. Read but not handled (like in Java).
 * <p>
 * Java: com.aionemu.chatserver.network.aion.clientpackets.CM_PLAYER_INFO
 *
 * @author Neon
 */
class CM_PLAYER_INFO : public AbstractClientPacket {
public:
	CM_PLAYER_INFO(common::netty::ChannelBuffer channelBuffer, std::shared_ptr<netty::handler::ClientChannelHandler> clientChannelHandler, int8_t opCode) noexcept
		: AbstractClientPacket(std::move(channelBuffer), std::move(clientChannelHandler), opCode) {}

protected:
	void readImpl() override;

	void runImpl() override {
		// TODO Find out what other information is sent, maybe handle it if it's useful
	}

private:
	[[maybe_unused]] int32_t classId = 0, level = 0;
	[[maybe_unused]] std::vector<uint8_t> unk;
};

} // namespace aion::chatserver::network::aion::clientpackets
