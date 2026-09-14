#include "aion/gameserver/network/aion/serverpackets/SM_ABYSS_ARTIFACT_INFO3.h"

#include "aion/gameserver/model/siege/ArtifactLocation.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_ABYSS_ARTIFACT_INFO3::SM_ABYSS_ARTIFACT_INFO3(const std::vector<runtime::Ptr<model::siege::ArtifactLocation>>& collection)
	: AionServerPacket(opcodeOf<SM_ABYSS_ARTIFACT_INFO3>), locations(collection.begin(), collection.end()) {
}

SM_ABYSS_ARTIFACT_INFO3::SM_ABYSS_ARTIFACT_INFO3(int32_t loc) : AionServerPacket(opcodeOf<SM_ABYSS_ARTIFACT_INFO3>) {
	AION_UNPORTED();
}

SM_ABYSS_ARTIFACT_INFO3::~SM_ABYSS_ARTIFACT_INFO3() = default;

void SM_ABYSS_ARTIFACT_INFO3::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
