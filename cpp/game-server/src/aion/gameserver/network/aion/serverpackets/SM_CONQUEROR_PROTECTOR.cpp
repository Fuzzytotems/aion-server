#include "aion/gameserver/network/aion/serverpackets/SM_CONQUEROR_PROTECTOR.h"

#include <vector>

#include "aion/gameserver/model/gameobjects/detail/ObjectsData.h"
#include "aion/gameserver/model/gameobjects/player/AbyssRank.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/serverpackets/AbstractPlayerInfoPacket.h"
#include "aion/gameserver/services/conquerorAndProtectorSystem/CPInfo.h"
#include "aion/gameserver/services/conquerorAndProtectorSystem/ConquerorAndProtectorService.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_CONQUEROR_PROTECTOR::SM_CONQUEROR_PROTECTOR(int32_t typeValue, int32_t buffLvlValue, int32_t cooldownValue)
	: AionServerPacket(opcodeOf<SM_CONQUEROR_PROTECTOR>), type(typeValue), buffLvl(buffLvlValue), cooldown(cooldownValue) {
}

SM_CONQUEROR_PROTECTOR::SM_CONQUEROR_PROTECTOR(int32_t typeValue, int32_t buffLvlValue)
	: AionServerPacket(opcodeOf<SM_CONQUEROR_PROTECTOR>), type(typeValue), buffLvl(buffLvlValue) {
}

SM_CONQUEROR_PROTECTOR::SM_CONQUEROR_PROTECTOR(int32_t typeValue, model::gameobjects::player::Player& playerValue)
	: AionServerPacket(opcodeOf<SM_CONQUEROR_PROTECTOR>), type(typeValue), player(playerValue) {
}

SM_CONQUEROR_PROTECTOR::SM_CONQUEROR_PROTECTOR(const std::vector<runtime::Ptr<model::gameobjects::player::Player>>& intrudersValue, bool displayCd)
	: AionServerPacket(opcodeOf<SM_CONQUEROR_PROTECTOR>), type(displayCd ? 5 : 4), intruders(intrudersValue.begin(), intrudersValue.end()) {
}

SM_CONQUEROR_PROTECTOR::~SM_CONQUEROR_PROTECTOR() = default;

void SM_CONQUEROR_PROTECTOR::writeImpl(AionConnection* con) {
	writeD(type);
	writeD(0x01);
	writeD(0x01);
	switch (type) {
		case 0: // conqueror + no announcement
		case 1: // conqueror + announcement
		case 7: // protector + cd no announcement
		case 8: // protector + announcement
			writeH(0x01);
			writeD(buffLvl);
			writeD(cooldown); // intruder scan cooldown
			break;
		case 4: // intruder scan (without cd)
		case 5: // intruder scan (with cd)
			writeH(static_cast<int32_t>(intruders.size()));
			for (const runtime::Ref<model::gameobjects::player::Player>& intruder : intruders) {
				runtime::Ptr<services::conquerorAndProtectorSystem::CPInfo> info =
					services::conquerorAndProtectorSystem::ConquerorAndProtectorService::getInstance().getCPInfoForCurrentMap(*intruder);
				writeD(!info ? 0 : info->getRank());
				writeD(intruder->getObjectId());
				writeD(0x01); // unk
				writeD(model::gameobjects::detail::abyssRankId(intruder->getAbyssRank()->getRank()));
				writeH(intruder->getLevel());
				writeF(intruder->getX());
				writeF(intruder->getY());
				writeS(intruder->getName(true), AbstractPlayerInfoPacket::CHARNAME_MAX_LENGTH);
				writeB(std::vector<uint8_t>(66)); // unk
				writeD(1941); // unk
				writeD(1942); // unk
				writeD(1943); // unk
				writeD(1944); // unk
				writeH(7); // unk
			}
			break;
		case 6: // conqueror
		case 9: { // protector
			writeH(0x01); // unk
			runtime::Ptr<services::conquerorAndProtectorSystem::CPInfo> info =
				services::conquerorAndProtectorSystem::ConquerorAndProtectorService::getInstance().getCPInfoForCurrentMap(*player);
			writeD(!info ? 0 : info->getRank());
			writeD(player->getObjectId());
			break;
		}
	}
}

} // namespace aion::gameserver::network::aion::serverpackets
