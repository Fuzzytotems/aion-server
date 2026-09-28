#pragma once

#include "aion/loginserver/network/gameserver/GsServerPacket.h"

namespace aion::loginserver::network::gameserver::serverpackets {

/**
 * The list of banned MAC addresses of BannedMacManager, read when the packet is written (Java: the live map; here a copy taken under the
 * manager's lock).
 * <p>
 * Java: com.aionemu.loginserver.network.gameserver.serverpackets.SM_MACBAN_LIST
 *
 * @author KID
 */
class SM_MACBAN_LIST : public GsServerPacket {
public:
	/** Java: the constructor loads the BannedMacManager (first use) */
	SM_MACBAN_LIST();

protected:
	void writeImpl(GsConnection& con, commons::utils::ByteBuffer& buf) const override;
};

} // namespace aion::loginserver::network::gameserver::serverpackets
