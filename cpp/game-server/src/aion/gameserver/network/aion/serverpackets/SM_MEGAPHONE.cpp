#include "aion/gameserver/network/aion/serverpackets/SM_MEGAPHONE.h"

#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/RaceInfo.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MEGAPHONE_FactionLabelInfo.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_MEGAPHONE::SM_MEGAPHONE(model::gameobjects::player::Player& sender, std::string_view messageValue, int32_t itemIdValue)
	: SM_MEGAPHONE(sender.getRace() == model::Race::ELYOS ? FactionLabel::ELYOS : FactionLabel::ASMODIANS, sender.getName(), messageValue, itemIdValue) {
}

SM_MEGAPHONE::SM_MEGAPHONE(SM_MEGAPHONE::FactionLabel senderFactionValue, std::string_view senderNameValue, std::string_view messageValue,
	int32_t itemIdValue)
	: AionServerPacket(opcodeOf<SM_MEGAPHONE>), senderFaction(senderFactionValue), senderName(senderNameValue), message(messageValue),
	  itemId(itemIdValue) {
}

void SM_MEGAPHONE::writeImpl(AionConnection* con) {
	writeS(senderName);
	writeS(message);
	writeD(itemId); // used by the client to determine message color
	writeC(id(senderFaction));
}

} // namespace aion::gameserver::network::aion::serverpackets
