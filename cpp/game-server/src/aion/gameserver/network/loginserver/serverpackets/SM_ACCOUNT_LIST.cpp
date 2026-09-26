#include "aion/gameserver/network/loginserver/serverpackets/SM_ACCOUNT_LIST.h"

#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/network/aion/AionConnection.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::loginserver::serverpackets {

SM_ACCOUNT_LIST::SM_ACCOUNT_LIST(std::vector<std::shared_ptr<network::aion::AionConnection>> accountsValue)
	: LsServerPacket(0x04), accounts(std::move(accountsValue)) {
}

SM_ACCOUNT_LIST::~SM_ACCOUNT_LIST() = default;

void SM_ACCOUNT_LIST::writeImpl(LoginServerConnection* con, commons::utils::ByteBuffer& buf) {
	writeD(buf, static_cast<int32_t>(accounts.size()));
	for (const std::shared_ptr<network::aion::AionConnection>& ac : accounts)
		writeD(buf, ac->getAccount()->getId());
}

} // namespace aion::gameserver::network::loginserver::serverpackets
