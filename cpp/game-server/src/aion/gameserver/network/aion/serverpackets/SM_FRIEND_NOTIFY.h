#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * Notifies players when their friends log in, out, or delete them
 *
 * @author Ben
 */
class SM_FRIEND_NOTIFY : public AionServerPacket {
public:
	static constexpr int8_t LOGIN = 0;
	static constexpr int8_t LOGOUT = 1;
	static constexpr int8_t DELETED = 2;

private:
	int8_t code{};
	std::string name{};

public:
	/** Constructs a new notify packet */
	SM_FRIEND_NOTIFY(int8_t code, std::string_view name);

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
