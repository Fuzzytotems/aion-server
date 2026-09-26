#include "aion/gameserver/model/team/league/League.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/AionObject.h"
#include "aion/gameserver/model/team/TeamMember.h"
#include "aion/gameserver/model/team/alliance/PlayerAlliance.h"
#include "aion/gameserver/model/team/common/legacy/LootGroupRules.h"

namespace aion::gameserver::model::team::league {

League::League(LeagueMember& value)
	: GeneralTeam(int32_t{}, bool{}), lootGroupRules(common::legacy::LootGroupRules::create()) {
	// Java: super(IDFactory.getInstance().nextId(), true); setLeader(leader); super(...) arguments
	AION_UNPORTED();
}

runtime::Ref<League> League::create(LeagueMember& value) {
	return runtime::makeRef<League>(value);
}

std::vector<runtime::Ptr<gameobjects::player::Player>> League::getOnlineMembers() {
	AION_UNPORTED();
}

void League::addMember(TeamMember& member) {
	AION_UNPORTED();
}

void League::onRemoveMember(TeamMember& member) {
	AION_UNPORTED();
}

int32_t League::getMaxMemberCount() {
	AION_UNPORTED();
}

void League::sendPackets(std::initializer_list<std::reference_wrapper<network::aion::AionServerPacket>> packets) {
	AION_UNPORTED();
}

void League::sendPacket(const std::function<bool(gameobjects::AionObject&)>& predicate,
	std::initializer_list<std::reference_wrapper<network::aion::AionServerPacket>> packets) {
	AION_UNPORTED();
}

Race League::getRace() {
	AION_UNPORTED();
}

runtime::Ptr<gameobjects::player::Player> League::getCaptain() {
	AION_UNPORTED();
}

void League::setLootGroupRules(runtime::Ptr<common::legacy::LootGroupRules> value) {
	this->lootGroupRules.set(value);
}

std::vector<runtime::Ptr<LeagueMember>> League::getSortedMembers() {
	AION_UNPORTED();
}

runtime::Ptr<gameobjects::player::Player> League::reorganize() {
	AION_UNPORTED();
}

runtime::Ptr<gameobjects::player::Player> League::getPlayerMember(int32_t playerObjId) {
	AION_UNPORTED();
}

void League::broadcast() {
	AION_UNPORTED();
}

void League::broadcast(gameobjects::player::Player& skippedPlayer) {
	AION_UNPORTED();
}

void League::broadcast(alliance::PlayerAlliance& skippedAlliance) {
	AION_UNPORTED();
}

void League::broadcast(runtime::Ptr<alliance::PlayerAlliance> skippedAlliance, runtime::Ptr<gameobjects::player::Player> skippedPlayer) {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<gameobjects::player::Player>> League::getCaptains() {
	AION_UNPORTED();
}

League::~League() = default;

runtime::Ptr<LeagueMember> League::getMember(int32_t value) {
	// port: runtime::cast<LeagueMember> of the GeneralTeam result, once model/team/league/LeagueMember.h exists
	AION_UNPORTED();
}

runtime::Ptr<LeagueMember> League::removeMember(TeamMember& member) {
	// port: runtime::cast<LeagueMember> of the GeneralTeam result, once model/team/league/LeagueMember.h exists
	AION_UNPORTED();
}

runtime::Ptr<LeagueMember> League::removeMember(int32_t value) {
	// port: runtime::cast<LeagueMember> of the GeneralTeam result, once model/team/league/LeagueMember.h exists
	AION_UNPORTED();
}

runtime::Ptr<LeagueMember> League::getLeader() const {
	// port: runtime::cast<LeagueMember> of the GeneralTeam result, once model/team/league/LeagueMember.h exists
	AION_UNPORTED();
}

runtime::Ptr<alliance::PlayerAlliance> League::getLeaderObject() {
	return runtime::cast<alliance::PlayerAlliance>(GeneralTeam::getLeaderObject());
}

} // namespace aion::gameserver::model::team::league
