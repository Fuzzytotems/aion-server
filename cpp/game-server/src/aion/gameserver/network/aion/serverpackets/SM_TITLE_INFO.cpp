#include "aion/gameserver/network/aion/serverpackets/SM_TITLE_INFO.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/player/title/TitleList.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_TITLE_INFO::SM_TITLE_INFO(model::gameobjects::player::Player& player)
	: AionServerPacket(opcodeOf<SM_TITLE_INFO>), action(0) {
	AION_UNPORTED();
}

SM_TITLE_INFO::SM_TITLE_INFO(int32_t titleIdValue)
	: AionServerPacket(opcodeOf<SM_TITLE_INFO>), action(1), titleId(titleIdValue) {
}

SM_TITLE_INFO::SM_TITLE_INFO(model::gameobjects::player::Player& player, int32_t titleIdValue)
	: AionServerPacket(opcodeOf<SM_TITLE_INFO>), action(3), titleId(titleIdValue) {
	AION_UNPORTED();
}

SM_TITLE_INFO::SM_TITLE_INFO(bool flag)
	: AionServerPacket(opcodeOf<SM_TITLE_INFO>), action(4), titleId(flag ? 1 : 0) {
}

SM_TITLE_INFO::SM_TITLE_INFO(model::gameobjects::player::Player& player, bool flag)
	: AionServerPacket(opcodeOf<SM_TITLE_INFO>), action(5), titleId(flag ? 1 : 0) {
	AION_UNPORTED();
}

SM_TITLE_INFO::SM_TITLE_INFO(int32_t actionValue, int32_t bonusTitleIdValue)
	: AionServerPacket(opcodeOf<SM_TITLE_INFO>), action(actionValue), bonusTitleId(bonusTitleIdValue) {
}

SM_TITLE_INFO::~SM_TITLE_INFO() = default;

void SM_TITLE_INFO::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
