#pragma once

#include "aion/loginserver/network/gameserver/GsServerPacket.h"

namespace aion::loginserver::network::gameserver::serverpackets {

/**
 * The list of banned HDD serials of BannedHDDController, read when the packet is written (Java: the live map; here a copy taken under the
 * controller's lock).
 * <p>
 * Java: com.aionemu.loginserver.network.gameserver.serverpackets.SM_HDDBAN_LIST
 *
 * @author ViAl
 */
class SM_HDDBAN_LIST : public GsServerPacket {
public:
	/** Java: the constructor loads the BannedHDDController (first use) */
	SM_HDDBAN_LIST();

protected:
	void writeImpl(GsConnection& con, commons::utils::ByteBuffer& buf) const override;
};

} // namespace aion::loginserver::network::gameserver::serverpackets
