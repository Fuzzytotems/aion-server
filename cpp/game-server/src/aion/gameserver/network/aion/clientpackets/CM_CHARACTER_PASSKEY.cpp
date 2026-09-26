#include "aion/gameserver/network/aion/clientpackets/CM_CHARACTER_PASSKEY.h"

#include <memory>
#include <string>
#include <vector>

#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/configs/main/SecurityConfig.h"
#include "aion/gameserver/dao/PlayerPasskeyDAO.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/account/CharacterPasskey.h"
#include "aion/gameserver/model/account/PlayerAccountData.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CHARACTER_SELECT.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DELETE_CHARACTER.h"
#include "aion/gameserver/network/loginserver/LoginServer.h"
#include "aion/gameserver/services/player/PlayerEnterWorldService.h"
#include "aion/gameserver/services/player/PlayerService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

namespace {

/** Java: new String(bytes, StandardCharsets.UTF_16LE) - every code unit is kept (NUL padding included), unpaired surrogates become U+FFFD */
std::string decodeUtf16Le(const std::vector<uint8_t>& bytes) {
	std::u16string text;
	text.reserve(bytes.size() / 2);
	for (size_t i = 0; i + 1 < bytes.size(); i += 2)
		text += static_cast<char16_t>(bytes[i] | bytes[i + 1] << 8);
	return commons::utils::StringUtils::toUtf8(text);
}

} // namespace

CM_CHARACTER_PASSKEY::CM_CHARACTER_PASSKEY(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

void CM_CHARACTER_PASSKEY::readImpl() {
	type = readH(); // 0:new, 2:update, 3:input
	passkey = decodeUtf16Le(readB(48));
	if (type == 2)
		newPasskey = decodeUtf16Le(readB(48));
}

void CM_CHARACTER_PASSKEY::runImpl() {
	using model::account::CharacterPasskey;
	using serverpackets::SM_CHARACTER_SELECT;
	const std::shared_ptr<AionConnection>& con = getConnection();
	runtime::Ptr<CharacterPasskey> chaPasskey = con->getAccount()->getCharacterPasskey();
	switch (type) {
		case 0:
			chaPasskey->setIsPass(false);
			chaPasskey->setWrongCount(0);
			dao::PlayerPasskeyDAO::insertPlayerPasskey(con->getAccount()->getId(), passkey);
			con->sendPacket(SM_CHARACTER_SELECT(2, type, chaPasskey->getWrongCount()));
			break;
		case 2: {
			bool isSuccess = dao::PlayerPasskeyDAO::updatePlayerPasskey(con->getAccount()->getId(), passkey, newPasskey);
			chaPasskey->setIsPass(false);
			if (isSuccess) {
				chaPasskey->setWrongCount(0);
				con->sendPacket(SM_CHARACTER_SELECT(2, type, chaPasskey->getWrongCount()));
			} else {
				chaPasskey->setWrongCount(chaPasskey->getWrongCount() + 1);
				checkBlock(con->getAccount()->getId(), chaPasskey->getWrongCount());
				con->sendPacket(SM_CHARACTER_SELECT(2, type, chaPasskey->getWrongCount()));
			}
			break;
		}
		case 3: {
			bool isPass = dao::PlayerPasskeyDAO::checkPlayerPasskey(con->getAccount()->getId(), passkey);
			if (isPass) {
				chaPasskey->setIsPass(true);
				chaPasskey->setWrongCount(0);
				con->sendPacket(SM_CHARACTER_SELECT(2, type, chaPasskey->getWrongCount()));
				// C++: CharacterPasskey.connectType defaults to ENTER, Java's null matches neither branch (a type 3 packet without a preceding enter or
				// delete request; see docs/deviations/P5-00.md)
				if (chaPasskey->getConnectType() == CharacterPasskey::ConnectType::ENTER)
					services::player::PlayerEnterWorldService::enterWorld(con.get(), chaPasskey->getObjectId());
				else if (chaPasskey->getConnectType() == CharacterPasskey::ConnectType::DELETE) {
					runtime::Ptr<model::account::PlayerAccountData> playerAccData = con->getAccount()->getPlayerAccountData(chaPasskey->getObjectId());
					services::player::PlayerService::deletePlayer(*playerAccData);
					con->sendPacket(serverpackets::SM_DELETE_CHARACTER(chaPasskey->getObjectId(), playerAccData->getDeletionTimeInSeconds()));
				}
			} else {
				chaPasskey->setIsPass(false);
				chaPasskey->setWrongCount(chaPasskey->getWrongCount() + 1);
				checkBlock(con->getAccount()->getId(), chaPasskey->getWrongCount());
				con->sendPacket(SM_CHARACTER_SELECT(2, type, chaPasskey->getWrongCount()));
			}
			break;
		}
		default:
			break;
	}
}

void CM_CHARACTER_PASSKEY::checkBlock(int32_t accountId, int32_t wrongCount) {
	if (wrongCount >= configs::main::SecurityConfig::PASSKEY_WRONG_MAXCOUNT.load()) {
		// TODO : Change the account to be blocked
		loginserver::LoginServer::getInstance().sendBanPacket(2, accountId, "", 60 * 8, 0);
	}
}

AION_CLIENT_PACKET(CM_CHARACTER_PASSKEY);

} // namespace aion::gameserver::network::aion::clientpackets
