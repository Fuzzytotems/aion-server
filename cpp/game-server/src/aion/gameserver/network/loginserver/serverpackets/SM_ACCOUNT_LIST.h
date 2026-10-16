#pragma once

#include <memory>
#include <vector>

#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/loginserver/LsServerPacket.h"
#include "aion/gameserver/network/loginserver/serverpackets/fwd.h"

namespace aion::gameserver::network::loginserver::serverpackets {

/**
 * GameServer packet that sends list of logged in accounts
 *
 * @author SoulKeeper, Neon
 */
class SM_ACCOUNT_LIST : public LsServerPacket {
private:
	/** Map with loaded accounts */
	std::vector<std::shared_ptr<network::aion::AionConnection>> accounts;

public:
	/** constructs new server packet with specified opcode. */
	explicit SM_ACCOUNT_LIST(std::vector<std::shared_ptr<network::aion::AionConnection>> accounts);
	~SM_ACCOUNT_LIST() override;

protected:
	void writeImpl(LoginServerConnection* con, commons::utils::ByteBuffer& buf) override;
};

} // namespace aion::gameserver::network::loginserver::serverpackets
