#pragma once

#include <cstdint>
#include <memory>

#include "aion/loginserver/network/aion/AionClientPacket.h"

namespace aion::loginserver::network::aion::clientpackets {

/**
 * Server list request: the character counts are requested from the game servers, then SM_SERVER_LIST is sent.
 * <p>
 * Java: com.aionemu.loginserver.network.aion.clientpackets.CM_SERVER_LIST
 *
 * @author -Nemesiss-
 */
class CM_SERVER_LIST : public AionClientPacket {
public:
	CM_SERVER_LIST(commons::utils::ByteBuffer buf, std::shared_ptr<LoginConnection> client, int32_t opCode)
		: AionClientPacket(std::move(buf), std::move(client), opCode) {}

protected:
	void readImpl() override;
	void runImpl() override;

private:
	/** accountId is part of session key - its used for security purposes */
	int32_t accountId = 0;
	/** loginOk is part of session key - its used for security purposes */
	int32_t loginOk = 0;
};

} // namespace aion::loginserver::network::aion::clientpackets
