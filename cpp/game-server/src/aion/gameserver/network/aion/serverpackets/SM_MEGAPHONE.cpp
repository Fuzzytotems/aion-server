#include "aion/gameserver/network/aion/serverpackets/SM_MEGAPHONE.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_MEGAPHONE::SM_MEGAPHONE(model::gameobjects::player::Player& sender, std::string_view messageValue, int32_t itemIdValue)
	: AionServerPacket(opcodeOf<SM_MEGAPHONE>) {
	AION_UNPORTED();
}

SM_MEGAPHONE::SM_MEGAPHONE(SM_MEGAPHONE::FactionLabel senderFactionValue, std::string_view senderNameValue, std::string_view messageValue,
	int32_t itemIdValue)
	: AionServerPacket(opcodeOf<SM_MEGAPHONE>), senderFaction(senderFactionValue), senderName(senderNameValue), message(messageValue),
	  itemId(itemIdValue) {
}

void SM_MEGAPHONE::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
