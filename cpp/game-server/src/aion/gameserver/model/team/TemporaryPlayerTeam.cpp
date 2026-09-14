#include "aion/gameserver/model/team/TemporaryPlayerTeam.h"

#include "aion/gameserver/runtime/base/Unported.h"

// Member types (docs/design/hub-headers.md §3.3): the constructor, the destructor and the Field<Ref> setter need LootGroupRules.h, which is not an
// S0b hub (P5-10). Not an S0b transition guard: P5-10 removes it when it adds the header.
#if __has_include("aion/gameserver/model/team/common/legacy/LootGroupRules.h")
#define AION_TEMPORARY_PLAYER_TEAM_MEMBER_TYPES 1
#include "aion/gameserver/model/team/common/legacy/LootGroupRules.h"
#else
#define AION_TEMPORARY_PLAYER_TEAM_MEMBER_TYPES 0
#endif

namespace aion::gameserver::model::team {

#if AION_TEMPORARY_PLAYER_TEAM_MEMBER_TYPES
TemporaryPlayerTeam::TemporaryPlayerTeam(int32_t objId, bool autoReleaseObjectId)
	: GeneralTeam(objId, autoReleaseObjectId), lootGroupRules(common::legacy::LootGroupRules::create()) {
}

TemporaryPlayerTeam::~TemporaryPlayerTeam() = default;
#endif

void TemporaryPlayerTeam::updateBrand(int32_t brandId, int32_t targetObjectId) {
	AION_UNPORTED();
}

void TemporaryPlayerTeam::sendBrands(gameobjects::player::Player& member) {
	AION_UNPORTED();
}

Race TemporaryPlayerTeam::getRace() {
	AION_UNPORTED();
}

void TemporaryPlayerTeam::sendPackets(std::initializer_list<std::reference_wrapper<network::aion::AionServerPacket>> packets) {
	AION_UNPORTED();
}

void TemporaryPlayerTeam::sendPacket(const std::function<bool(gameobjects::AionObject&)>& predicate,
	std::initializer_list<std::reference_wrapper<network::aion::AionServerPacket>> packets) {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<gameobjects::player::Player>> TemporaryPlayerTeam::getOnlineMembers() {
	AION_UNPORTED();
}

void TemporaryPlayerTeam::setLootGroupRules(runtime::Ptr<common::legacy::LootGroupRules> lootGroupRulesValue) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::team
