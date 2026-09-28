#include "aion/gameserver/network/aion/serverpackets/SM_LEGION_INFO.h"

#include <string>
#include <vector>

#include "aion/gameserver/model/team/legion/Legion.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/serverpackets/detail/PacketLookups.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_LEGION_INFO::SM_LEGION_INFO(model::team::legion::Legion& legionValue)
	: SM_LEGION_INFO(opcodeOf<SM_LEGION_INFO>, legionValue) {
}

SM_LEGION_INFO::SM_LEGION_INFO(int32_t opCode, model::team::legion::Legion& legionValue)
	: AionServerPacket(opCode), legion(legionValue) {
}

SM_LEGION_INFO::~SM_LEGION_INFO() = default;

void SM_LEGION_INFO::writeImpl(AionConnection* con) {
	writeS(legion->getName());
	writeC(legion->getLegionLevel());
	writeD(detail::getRankingListPosition(*legion));
	writeH(legion->getDeputyPermission());
	writeH(legion->getCenturionPermission());
	writeH(legion->getLegionaryPermission());
	writeH(legion->getVolunteerPermission());
	writeQ(legion->getContributionPoints());
	writeD(0x00); // unk
	writeD(0x00); // unk
	writeD(legion->getDisbandTime());
	writeD(legion->getOccupiedLegionDominion());
	writeD(legion->getLastLegionDominion());
	writeD(legion->getCurrentLegionDominion());
	writeAnnouncements();
}

void SM_LEGION_INFO::writeAnnouncements() {
	// Java: Collections.singletonList(legion.getAnnouncement()), which may hold null
	const std::vector<runtime::Ptr<model::team::legion::Legion::Announcement>> announcements{legion->getAnnouncement()};
	for (size_t i = 0; i < 7; i++) {
		runtime::Ptr<model::team::legion::Legion::Announcement> announcement = i < announcements.size() ? announcements[i] : nullptr;
		writeS(announcement == nullptr ? std::string() : announcement->message());
		if (announcement == nullptr || announcement->message().empty()) // empty string is a stop marker
			break;
		writeD(static_cast<int32_t>(announcement->time().time_since_epoch().count() / 1000));
	}
}

} // namespace aion::gameserver::network::aion::serverpackets
