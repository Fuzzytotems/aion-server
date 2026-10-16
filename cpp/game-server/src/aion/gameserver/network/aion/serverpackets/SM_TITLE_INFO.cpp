#include "aion/gameserver/network/aion/serverpackets/SM_TITLE_INFO.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/title/Title.h"
#include "aion/gameserver/model/gameobjects/player/title/TitleList.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_TITLE_INFO::SM_TITLE_INFO(model::gameobjects::player::Player& player)
	: AionServerPacket(opcodeOf<SM_TITLE_INFO>), action(0) {
	titleList = runtime::Ref<model::gameobjects::player::title::TitleList>(player.getTitleList());
}

SM_TITLE_INFO::SM_TITLE_INFO(int32_t titleIdValue)
	: AionServerPacket(opcodeOf<SM_TITLE_INFO>), action(1), titleId(titleIdValue) {
}

SM_TITLE_INFO::SM_TITLE_INFO(model::gameobjects::player::Player& player, int32_t titleIdValue)
	: AionServerPacket(opcodeOf<SM_TITLE_INFO>), action(3), titleId(titleIdValue) {
	playerObjId = player.getObjectId();
}

SM_TITLE_INFO::SM_TITLE_INFO(bool flag)
	: AionServerPacket(opcodeOf<SM_TITLE_INFO>), action(4), titleId(flag ? 1 : 0) {
}

SM_TITLE_INFO::SM_TITLE_INFO(model::gameobjects::player::Player& player, bool flag)
	: AionServerPacket(opcodeOf<SM_TITLE_INFO>), action(5), titleId(flag ? 1 : 0) {
	playerObjId = player.getObjectId();
}

SM_TITLE_INFO::SM_TITLE_INFO(int32_t actionValue, int32_t bonusTitleIdValue)
	: AionServerPacket(opcodeOf<SM_TITLE_INFO>), action(actionValue), bonusTitleId(bonusTitleIdValue) {
}

SM_TITLE_INFO::~SM_TITLE_INFO() = default;

void SM_TITLE_INFO::writeImpl(AionConnection* con) {
	writeC(action);
	switch (action) {
		case 0:
			writeC(0x00);
			writeH(titleList->size());
			for (const runtime::Ptr<model::gameobjects::player::title::Title>& title : titleList->getTitles()) {
				writeD(title->getId());
				writeD(title->secondsUntilExpiration());
			}
			break;
		case 1: // self set
			writeH(titleId);
			break;
		case 3: // broad set
			writeD(playerObjId);
			writeH(titleId);
			break;
		case 4: // Mentor flag self
			writeH(titleId);
			break;
		case 5: // broad set mentor fleg
			writeD(playerObjId);
			writeH(titleId);
			break;
		case 6: // Title wich will take BonusStats from
			writeH(bonusTitleId);
	}
}

} // namespace aion::gameserver::network::aion::serverpackets
