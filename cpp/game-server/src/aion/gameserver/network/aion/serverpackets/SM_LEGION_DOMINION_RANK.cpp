#include "aion/gameserver/network/aion/serverpackets/SM_LEGION_DOMINION_RANK.h"

#include <algorithm>
#include <cstddef>

#include "aion/gameserver/model/legionDominion/LegionDominionLocation.h"
#include "aion/gameserver/model/legionDominion/LegionDominionParticipantInfo.h"
#include "aion/gameserver/model/team/legion/Legion.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_LEGION_DOMINION_RANK::SM_LEGION_DOMINION_RANK(model::legionDominion::LegionDominionLocation& locValue,
	runtime::Ptr<model::team::legion::Legion> legion)
	: AionServerPacket(opcodeOf<SM_LEGION_DOMINION_RANK>), loc(locValue) {
	using model::legionDominion::LegionDominionParticipantInfo;
	std::vector<runtime::Ptr<LegionDominionParticipantInfo>> ranking = locValue.getLegionRanking(false);
	runtime::Ptr<LegionDominionParticipantInfo> participant = legion == nullptr ? nullptr : locValue.getParticipantInfo(legion->getLegionId());
	// Java ranking.indexOf(participant) + 1: the class does not override equals, so the index of the same object (-1 + 1 if absent)
	auto found = participant == nullptr ? ranking.end() : std::ranges::find(ranking, participant);
	rank = participant == nullptr ? 0 : found == ranking.end() ? 0 : static_cast<int32_t>(found - ranking.begin()) + 1;
	// Java: ranking.size() > 25 ? ranking.subList(0, 25) : ranking
	size_t topSize = std::min<size_t>(ranking.size(), 25);
	topParticipants.assign(ranking.begin(), ranking.begin() + static_cast<std::ptrdiff_t>(topSize));
	if (static_cast<size_t>(rank) > topParticipants.size()) // if the ranked legion is not top-ranked, the last entry must be the ranked one
		topParticipants.back() = runtime::Ref<LegionDominionParticipantInfo>(ranking[static_cast<size_t>(rank - 1)]);
}

SM_LEGION_DOMINION_RANK::~SM_LEGION_DOMINION_RANK() = default;

void SM_LEGION_DOMINION_RANK::writeImpl(AionConnection* con) {
	writeD(loc->getLocationId());
	writeC(rank);
	writeH(static_cast<int32_t>(topParticipants.size()));
	for (const runtime::Ref<model::legionDominion::LegionDominionParticipantInfo>& participant : topParticipants) {
		writeD(participant->getPoints());
		writeD(participant->getTime());
		writeQ(participant->getDate());
		writeS(participant->getLegionName());
	}
}

} // namespace aion::gameserver::network::aion::serverpackets
