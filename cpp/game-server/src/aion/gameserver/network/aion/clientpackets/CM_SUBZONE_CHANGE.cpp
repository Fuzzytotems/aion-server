#include "aion/gameserver/network/aion/clientpackets/CM_SUBZONE_CHANGE.h"

#include <memory>
#include <string>

#include "aion/gameserver/configs/administration/AdminConfig.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/geometry/Area.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/zone/ZoneClassName.h"
#include "aion/gameserver/model/templates/zone/ZoneTemplate.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/world/zone/ZoneInstance.h"
#include "aion/gameserver/world/zone/ZoneName.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

using model::templates::zone::ZoneClassName;

CM_SUBZONE_CHANGE::CM_SUBZONE_CHANGE(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

void CM_SUBZONE_CHANGE::readImpl() {
	// Always 1, maybe for neutral zones 0 ?
	unk = readC();
}

void CM_SUBZONE_CHANGE::runImpl() {
	runtime::Ptr<model::gameobjects::player::Player> player = getConnection()->getActivePlayer();
	player->revalidateZones();
	if (player->hasAccess(configs::administration::AdminConfig::ZONE_INFO.load())) {
		int32_t foundZones = 0;
		for (const runtime::Ptr<world::zone::ZoneInstance>& zone : player->findZones()) {
			if (zone->getZoneTemplate()->getZoneType() == ZoneClassName::DUMMY || zone->getZoneTemplate()->getZoneType() == ZoneClassName::WEATHER)
				continue;
			foundZones++;
			// Java string concatenation: the byte as a number and the enum constant's name()
			utils::PacketSendUtility::sendMessage(*player, "Passed zone: unk=" + std::to_string(static_cast<int32_t>(unk)) + "; " +
				std::string(xml::enumName(zone->getZoneTemplate()->getZoneType())) + " " + zone->getAreaTemplate()->getZoneName()->name());
		}
		if (foundZones == 0) {
			utils::PacketSendUtility::sendMessage(*player, "Passed unknown zone, unk=" + std::to_string(static_cast<int32_t>(unk)));
			return;
		}
	}
}

AION_CLIENT_PACKET(CM_SUBZONE_CHANGE);

} // namespace aion::gameserver::network::aion::clientpackets
