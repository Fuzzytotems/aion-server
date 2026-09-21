#include "aion/gameserver/network/aion/clientpackets/CM_DELETE_CHARACTER.h"

#include "aion/gameserver/configs/main/SecurityConfig.h"
#include "aion/gameserver/dao/PlayerPasskeyDAO.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/account/CharacterPasskey.h"
#include "aion/gameserver/model/account/PlayerAccountData.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CHARACTER_SELECT.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DELETE_CHARACTER.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/LegionService.h"
#include "aion/gameserver/services/player/PlayerService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_DELETE_CHARACTER::CM_DELETE_CHARACTER(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

void CM_DELETE_CHARACTER::readImpl() {
	playOk2 = readD();
	chaOid = readD();
}

void CM_DELETE_CHARACTER::runImpl() {
	runtime::Ptr<model::account::Account> account = getConnection()->getAccount();
	runtime::Ptr<model::account::PlayerAccountData> playerAccData = account->getPlayerAccountData(chaOid);
	if (!playerAccData)
		return;
	if (services::LegionService::getInstance().getLegionMember(*playerAccData->getPlayerCommonData())) {
		sendPacket(serverpackets::SM_SYSTEM_MESSAGE::STR_GUILD_DISPERSE_STAYMODE_CANCEL_1());
		return;
	}
	// passkey check
	if (configs::main::SecurityConfig::PASSKEY_ENABLE.load() && !account->getCharacterPasskey()->isPass()) {
		account->getCharacterPasskey()->setConnectType(model::account::CharacterPasskey::ConnectType::DELETE);
		account->getCharacterPasskey()->setObjectId(chaOid);
		bool hasPasskey = dao::PlayerPasskeyDAO::existCheckPlayerPasskey(account->getId());
		sendPacket(serverpackets::SM_CHARACTER_SELECT(hasPasskey ? 1 : 0));
	} else {
		services::player::PlayerService::deletePlayer(*playerAccData);
		sendPacket(serverpackets::SM_DELETE_CHARACTER(chaOid, playerAccData->getDeletionTimeInSeconds()));
	}
}

AION_CLIENT_PACKET(CM_DELETE_CHARACTER);

} // namespace aion::gameserver::network::aion::clientpackets
