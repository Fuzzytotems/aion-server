#pragma once

#include "aion/loginserver/network/aion/AionServerPacket.h"

namespace aion::loginserver::network::aion::serverpackets {

/**
 * The game server list with the number of characters of the account on each server. The data is read while the packet is written (like Java),
 * from the GameServerTable, the connection's account and AccountController's character counts; all of these are leaf locks, so writing it with
 * the connection's guard held cannot deadlock.
 * <p>
 * Java: com.aionemu.loginserver.network.aion.serverpackets.SM_SERVER_LIST
 *
 * @author -Nemesiss-, cura, Neon
 */
class SM_SERVER_LIST : public AionServerPacket {
public:
	SM_SERVER_LIST() noexcept : AionServerPacket(0x04) {}

protected:
	/** @throws commons::utils::IllegalStateException if the connection has no account (Java: NullPointerException) */
	void writeImpl(LoginConnection& con, commons::utils::ByteBuffer& buf) const override;
};

} // namespace aion::loginserver::network::aion::serverpackets
