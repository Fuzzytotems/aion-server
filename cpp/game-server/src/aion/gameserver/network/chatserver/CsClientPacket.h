#pragma once

#include <concepts>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <utility>

#include "aion/commons/network/packet/BaseClientPacket.h"
#include "aion/gameserver/network/chatserver/fwd.h"

namespace aion::gameserver::network::chatserver {

/**
 * Base class of the packets the chat server sends. Read on the IO strand, run in receive order on the connection's serial executor
 * (runtime-architecture.md §1.4; Java: ThreadPoolManager.execute).
 *
 * @author ATracer
 */
class CsClientPacket : public commons::network::packet::BaseClientPacket<ChatServerConnection> {
protected:
	/**
	 * Constructs new client packet with specified opcode. If using this constructor, user must later manually set buffer and connection.
	 *
	 * @param opcode
	 *          packet id
	 */
	explicit CsClientPacket(int32_t opcode);

public:
	/** run runImpl catching and logging Throwable. Java final. */
	void run() override final;

protected:
	/** Send new CsServerPacket to connection that is owner of this packet. This method is equivalent to: getConnection().sendPacket(msg); */
	void sendPacket(std::shared_ptr<CsServerPacket> msg);

	/** C++ only: sendPacket for packet temporaries */
	template <std::derived_from<CsServerPacket> P>
	void sendPacket(P&& msg) {
		sendPacket(std::shared_ptr<CsServerPacket>(std::make_shared<std::remove_cvref_t<P>>(std::forward<P>(msg))));
	}

public:
	/**
	 * Clones this packet object.
	 * <p>
	 * C++: returns nullptr (Java's result for CloneNotSupportedException): a polymorphic packet has no shallow copy, and nothing calls it.
	 *
	 * @return CsClientPacket
	 */
	std::unique_ptr<CsClientPacket> clonePacket();
};

} // namespace aion::gameserver::network::chatserver
