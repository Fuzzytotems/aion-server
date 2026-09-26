#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"

#include "aion/gameserver/model/DialogPage.h"
#include "aion/gameserver/model/DialogPageInfo.h"
#include "aion/gameserver/model/gameobjects/player/Mailbox.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/TownService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::serverpackets {

SM_DIALOG_WINDOW::SM_DIALOG_WINDOW(int32_t targetObjectIdValue, int32_t dialogPageIdValue)
	: SM_DIALOG_WINDOW(targetObjectIdValue, dialogPageIdValue, 0) {
}

SM_DIALOG_WINDOW::SM_DIALOG_WINDOW(int32_t targetObjectIdValue, int32_t dialogPageIdValue, int32_t questIdValue)
	: AionServerPacket(opcodeOf<SM_DIALOG_WINDOW>), targetObjectId(targetObjectIdValue), dialogPageId(dialogPageIdValue), questId(questIdValue) {
}

void SM_DIALOG_WINDOW::writeImpl(AionConnection* con) {
	if (con == nullptr)
		throw runtime::NullPointerException("SM_DIALOG_WINDOW::writeImpl without a connection");
	runtime::Ptr<model::gameobjects::player::Player> player = con->getActivePlayer();
	writeD(targetObjectId);
	writeH(dialogPageId);
	writeD(questId);
	writeH(0);
	if (dialogPageId == model::id(model::DialogPage::MAIL)) {
		writeH(player->getMailbox()->mailBoxState.get());
	} else if (dialogPageId == model::id(model::DialogPage::TOWN_CHALLENGE_TASK)) {
		writeH(services::TownService::getInstance().getTownIdByPosition(*player));
	} else
		writeH(0);
}

} // namespace aion::gameserver::network::aion::serverpackets
