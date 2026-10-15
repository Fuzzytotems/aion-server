#include "aion/gameserver/network/aion/serverpackets/SM_LEGION_EDIT.h"

#include <chrono>

#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/team/legion/Legion.h"
#include "aion/gameserver/model/team/legion/LegionWarehouse.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/serverpackets/detail/PacketLookups.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_LEGION_EDIT::SM_LEGION_EDIT(int32_t typeValue)
	: AionServerPacket(opcodeOf<SM_LEGION_EDIT>), type(typeValue) {
}

SM_LEGION_EDIT::SM_LEGION_EDIT(int32_t typeValue, model::team::legion::Legion& legionValue)
	: AionServerPacket(opcodeOf<SM_LEGION_EDIT>), type(typeValue), legion(legionValue) {
}

SM_LEGION_EDIT::SM_LEGION_EDIT(int32_t typeValue, int32_t unixTimeValue)
	: AionServerPacket(opcodeOf<SM_LEGION_EDIT>), type(typeValue), unixTime(unixTimeValue) {
}

SM_LEGION_EDIT::SM_LEGION_EDIT(model::team::legion::Legion::Announcement& announcementValue)
	: AionServerPacket(opcodeOf<SM_LEGION_EDIT>), type(0x05) {
	announcement = announcementValue.message();
	// Java: (int) (announcement.time().getTime() / 1000)
	unixTime = static_cast<int32_t>(announcementValue.time().time_since_epoch().count() / 1000);
}

SM_LEGION_EDIT::~SM_LEGION_EDIT() = default;

void SM_LEGION_EDIT::writeImpl(AionConnection* con) {
	writeC(type);
	switch (type) {
		case 0x00: // Change Legion Level
			writeC(legion->getLegionLevel());
			break;
		case 0x01: // Change Abyss Ranking List Position
			writeD(detail::getRankingListPosition(*legion));
			break;
		case 0x02: // Change Legion Permissions
			writeH(legion->getDeputyPermission());
			writeH(legion->getCenturionPermission());
			writeH(legion->getLegionaryPermission());
			writeH(legion->getVolunteerPermission());
			break;
		case 0x03: // Change Legion Contributions
			writeQ(legion->getContributionPoints());
			break;
		case 0x04:
			writeQ(legion->getLegionWarehouse().getKinah());
			break;
		case 0x05: // Change Legion Announcement
			writeS(announcement);
			writeD(unixTime);
			break;
		case 0x06: // Disband Legion
			writeD(unixTime);
			break;
		case 0x07: // Recover Legion
			break;
		case 0x08: // Refresh Legion Announcement?
			break;
	}
}

} // namespace aion::gameserver::network::aion::serverpackets
