#include "aion/gameserver/model/team/TemporaryPlayerTeam.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/common/legacy/LootGroupRules.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::model::team {

TemporaryPlayerTeam::TemporaryPlayerTeam(int32_t objId, bool autoReleaseObjectId)
	: GeneralTeam(objId, autoReleaseObjectId), lootGroupRules(common::legacy::LootGroupRules::create()) {
}

TemporaryPlayerTeam::~TemporaryPlayerTeam() = default;

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

runtime::Ptr<gameobjects::player::Player> TemporaryPlayerTeam::getLeaderObject() {
	return runtime::cast<gameobjects::player::Player>(GeneralTeam::getLeaderObject());
}

} // namespace aion::gameserver::model::team
