#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_DIALOG_WINDOW::SM_DIALOG_WINDOW(int32_t targetObjectIdValue, int32_t dialogPageIdValue)
	: SM_DIALOG_WINDOW(targetObjectIdValue, dialogPageIdValue, 0) {
}

SM_DIALOG_WINDOW::SM_DIALOG_WINDOW(int32_t targetObjectIdValue, int32_t dialogPageIdValue, int32_t questIdValue)
	: AionServerPacket(opcodeOf<SM_DIALOG_WINDOW>), targetObjectId(targetObjectIdValue), dialogPageId(dialogPageIdValue), questId(questIdValue) {
}

void SM_DIALOG_WINDOW::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
