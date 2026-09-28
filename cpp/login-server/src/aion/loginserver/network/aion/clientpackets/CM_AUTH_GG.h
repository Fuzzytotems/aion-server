#pragma once

#include <cstdint>
#include <memory>

#include "aion/loginserver/network/aion/AionClientPacket.h"

namespace aion::loginserver::network::aion::clientpackets {

/**
 * GameGuard authentication, the first packet of a client. The session id must match the one sent in SM_INIT.
 * <p>
 * Java: com.aionemu.loginserver.network.aion.clientpackets.CM_AUTH_GG
 *
 * @author -Nemesiss-
 */
class CM_AUTH_GG : public AionClientPacket {
public:
	/** Constructs new instance of <tt>CM_AUTH_GG</tt> packet. */
	CM_AUTH_GG(commons::utils::ByteBuffer buf, std::shared_ptr<LoginConnection> client, int32_t opCode)
		: AionClientPacket(std::move(buf), std::move(client), opCode) {}

protected:
	void readImpl() override;
	void runImpl() override;

private:
	/** session id - its should match sessionId that was send in Init packet. */
	int32_t sessionId = 0;
};

} // namespace aion::loginserver::network::aion::clientpackets
