#pragma once

#include <cstdint>
#include <vector>

#include "aion/loginserver/network/gameserver/GsClientPacket.h"

namespace aion::loginserver::network::gameserver::clientpackets {

/**
 * Reads the list of account id's that are logged in to game server. This packet is sent by game server once it successfully registered on this login
 * server.
 * <p>
 * Java: com.aionemu.loginserver.network.gameserver.clientpackets.CM_ACCOUNT_LIST
 *
 * @author SoulKeeper, Neon
 */
class CM_ACCOUNT_LIST : public GsClientPacket {
protected:
	/**
	 * @throws commons::utils::IllegalArgumentException if the count is negative (Java: NegativeArraySizeException) or larger than the remaining
	 *           data allows. Deviation: Java allocates the array for any count, reading zeros past the end (or running out of memory).
	 */
	void readImpl() override;
	void runImpl() override;

private:
	/** Array with accounts that are logged in */
	std::vector<int32_t> accountIds;
};

} // namespace aion::loginserver::network::gameserver::clientpackets
