#pragma once

#include <utility>

#include "aion/chatserver/model/message/Message.h"
#include "aion/chatserver/network/aion/AbstractServerPacket.h"

namespace aion::chatserver::network::aion::serverpackets {

/**
 * A chat message in a channel: the channel id, the sender's id and "name@identifier" and the text.
 * <p>
 * Java: com.aionemu.chatserver.network.aion.serverpackets.SM_CHANNEL_MESSAGE
 *
 * @author ATracer
 */
class SM_CHANNEL_MESSAGE : public AbstractServerPacket {
public:
	/** @param message copied (Java keeps the reference; the packet is written right away, see ClientChannelHandler::sendPacket) */
	explicit SM_CHANNEL_MESSAGE(model::message::Message message) : AbstractServerPacket(0x1A), message(std::move(message)) {}

protected:
	/** @throws commons::utils::IllegalStateException if the sender has no identifier yet (Java: NullPointerException) */
	void writeImpl(netty::handler::ClientChannelHandler* cHandler, common::netty::ChannelBuffer& buf) const override;

private:
	const model::message::Message message;
};

} // namespace aion::chatserver::network::aion::serverpackets
