#include "aion/gameserver/network/aion/serverpackets/AbstractPlayerInfoPacket.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/account/CharacterBanInfo.h"

namespace aion::gameserver::network::aion::serverpackets {

AbstractPlayerInfoPacket::AbstractPlayerInfoPacket(int32_t opCode) : AionServerPacket(opCode) {
}

void AbstractPlayerInfoPacket::writePlayerInfo(model::account::PlayerAccountData& accPlData, AionConnection* con) {
	AION_UNPORTED();
}

void AbstractPlayerInfoPacket::writeEquippedItems(const std::vector<runtime::Ptr<model::gameobjects::Item>>& items) {
	AION_UNPORTED();
}

runtime::Ref<model::account::CharacterBanInfo> AbstractPlayerInfoPacket::getCharBanInfo(model::account::PlayerAccountData& playerAccountData,
	AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
