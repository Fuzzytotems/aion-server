#include "aion/gameserver/network/aion/serverpackets/SM_ABYSS_ARTIFACT_INFO3.h"

#include "aion/gameserver/model/siege/ArtifactLocation.h"
#include "aion/gameserver/model/siege/ArtifactStatus.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/services/SiegeService.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_ABYSS_ARTIFACT_INFO3::SM_ABYSS_ARTIFACT_INFO3(const std::vector<runtime::Ptr<model::siege::ArtifactLocation>>& collection)
	: AionServerPacket(opcodeOf<SM_ABYSS_ARTIFACT_INFO3>), locations(collection.begin(), collection.end()) {
}

SM_ABYSS_ARTIFACT_INFO3::SM_ABYSS_ARTIFACT_INFO3(int32_t loc) : AionServerPacket(opcodeOf<SM_ABYSS_ARTIFACT_INFO3>) {
	locations.emplace_back(services::SiegeService::getInstance().getArtifact(loc));
}

SM_ABYSS_ARTIFACT_INFO3::~SM_ABYSS_ARTIFACT_INFO3() = default;

void SM_ABYSS_ARTIFACT_INFO3::writeImpl(AionConnection* con) {
	writeH(static_cast<int32_t>(locations.size()));
	for (const runtime::Ref<model::siege::ArtifactLocation>& artifact : locations) {
		runtime::Ptr<model::siege::ArtifactLocation> location = artifact; // Java: null elements throw NullPointerException
		writeD(location->getLocationId() * 10 + 1);
		writeC(static_cast<int32_t>(location->getStatus())); // Java: ArtifactStatus.getValue() equals the ordinal (IDLE 0 .. ACTIVATED 3)
		writeD(0);
	}
}

} // namespace aion::gameserver::network::aion::serverpackets
