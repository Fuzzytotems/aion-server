#pragma once

#include <cstdint>

#include "aion/loginserver/network/gameserver/GsClientPacket.h"

namespace aion::loginserver::network::gameserver::clientpackets {

/**
 * Changes the access level (type 1) or membership (type 2) of an account.
 * <p>
 * Java: com.aionemu.loginserver.network.gameserver.clientpackets.CM_LS_CONTROL
 *
 * @author Aionchs-Wylovech
 */
class CM_LS_CONTROL : public GsClientPacket {
protected:
	void readImpl() override;
	/** @throws commons::utils::IllegalStateException if the account does not exist (Java: NullPointerException, no response) */
	void runImpl() override;

private:
	int8_t type = 0, param = 0;
	int32_t accountId = 0, adminId = 0;
};

} // namespace aion::loginserver::network::gameserver::clientpackets
