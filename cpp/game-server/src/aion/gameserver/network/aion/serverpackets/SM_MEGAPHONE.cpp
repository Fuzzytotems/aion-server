#include "aion/gameserver/network/aion/serverpackets/SM_MEGAPHONE.h"

#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/RaceInfo.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

namespace {

/** Java: SM_MEGAPHONE.FactionLabel.id - NONE((byte) -1), ELYOS((byte) Race.ELYOS.getRaceId()), ASMODIANS((byte) Race.ASMODIANS.getRaceId()) */
int8_t factionLabelId(SM_MEGAPHONE::FactionLabel label) {
	switch (label) {
		case SM_MEGAPHONE::FactionLabel::ELYOS:
			return static_cast<int8_t>(model::getRaceId(model::Race::ELYOS));
		case SM_MEGAPHONE::FactionLabel::ASMODIANS:
			return static_cast<int8_t>(model::getRaceId(model::Race::ASMODIANS));
		case SM_MEGAPHONE::FactionLabel::NONE:
			break;
	}
	return -1;
}

} // namespace

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
	writeC(factionLabelId(senderFaction));
}

} // namespace aion::gameserver::network::aion::serverpackets
