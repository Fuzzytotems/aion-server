#include "aion/gameserver/network/aion/serverpackets/SM_ALLIANCE_INFO.h"

#include "aion/gameserver/model/team/alliance/PlayerAlliance.h"
#include "aion/gameserver/model/team/common/legacy/LootGroupRules.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::network::aion::serverpackets {

runtime::Ref<SM_ALLIANCE_INFO::AllianceInfo> SM_ALLIANCE_INFO::AllianceInfo::create() {
	return runtime::makeRef<AllianceInfo>();
}

SM_ALLIANCE_INFO::AllianceInfo::AllianceInfo() = default;

SM_ALLIANCE_INFO::AllianceInfo::~AllianceInfo() = default;

SM_ALLIANCE_INFO::SM_ALLIANCE_INFO(model::team::alliance::PlayerAlliance& allianceValue) : SM_ALLIANCE_INFO(allianceValue, 0, "", nullptr) {
}

SM_ALLIANCE_INFO::SM_ALLIANCE_INFO(model::team::alliance::PlayerAlliance& allianceValue, model::team::alliance::PlayerAlliance& skipped)
	: SM_ALLIANCE_INFO(allianceValue, 0, "", skipped) {
}

SM_ALLIANCE_INFO::SM_ALLIANCE_INFO(model::team::alliance::PlayerAlliance& allianceValue, int32_t messageIdValue, std::string_view messageValue)
	: SM_ALLIANCE_INFO(allianceValue, messageIdValue, messageValue, nullptr) {
}

SM_ALLIANCE_INFO::SM_ALLIANCE_INFO(model::team::alliance::PlayerAlliance& allianceValue, int32_t messageIdValue, std::string_view messageValue,
	runtime::Ptr<model::team::alliance::PlayerAlliance> skipped)
	: AionServerPacket(opcodeOf<SM_ALLIANCE_INFO>) {
	AION_UNPORTED();
}

SM_ALLIANCE_INFO::~SM_ALLIANCE_INFO() = default;

void SM_ALLIANCE_INFO::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
