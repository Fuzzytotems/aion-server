#pragma once

#include <array>
#include <cstdint>

#include "aion/loginserver/network/aion/AionServerPacket.h"
#include "aion/loginserver/network/aion/SessionKey.h"

namespace aion::loginserver::network::aion::serverpackets {

/**
 * The client may connect to the selected game server: sends the playOk parts of the session key, which the game server verifies.
 * <p>
 * Java: com.aionemu.loginserver.network.aion.serverpackets.SM_PLAY_OK
 *
 * @author -Nemesiss-
 */
class SM_PLAY_OK : public AionServerPacket {
public:
	/** @param key session key */
	SM_PLAY_OK(const SessionKey& key, int8_t serverId) noexcept
		: AionServerPacket(0x07), playOk1(key.playOk1), playOk2(key.playOk2), serverId(serverId) {}

protected:
	void writeImpl(LoginConnection& con, commons::utils::ByteBuffer& buf) const override {
		writeD(buf, playOk1);
		writeD(buf, playOk2);
		writeC(buf, serverId);
		writeB(buf, std::array<uint8_t, 0x0E>{});
	}

private:
	/** playOk1 is part of session key - its used for security purposes [checked at game server side] */
	const int32_t playOk1;
	/** playOk2 is part of session key - its used for security purposes [checked at game server side] */
	const int32_t playOk2;
	const int32_t serverId;
};

} // namespace aion::loginserver::network::aion::serverpackets
