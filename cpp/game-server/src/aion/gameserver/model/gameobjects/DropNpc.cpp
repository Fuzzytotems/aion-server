#include "aion/gameserver/model/gameobjects/DropNpc.h"

#include "aion/gameserver/model/team/alliance/PlayerAlliance.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/TemporaryPlayerTeam.h"
#include "aion/gameserver/model/team/common/legacy/LootGroupRules.h"

namespace aion::gameserver::model::gameobjects {

DropNpc::DropNpc(int32_t value)
	: objectIdId(value), allowedLooters(runtime::RcHashSet<int32_t>::create(AION_LOCK_CLASS(DropNpc::allowedLooters))),
	  inRangePlayers(runtime::RcArrayList<runtime::Ref<player::Player>>::create(AION_LOCK_CLASS(DropNpc::inRangePlayers))) {
}

runtime::Ref<DropNpc> DropNpc::create(int32_t value) {
	return runtime::makeRef<DropNpc>(value);
}

void DropNpc::setAllowedLooters(runtime::Ptr<runtime::RcHashSet<int32_t>> value) {
	allowedLooters.set(value);
}

void DropNpc::setAllowedLooter(player::Player& player) {
	allowedLooters->add(player.getObjectId());
}

bool DropNpc::isAllowedToLoot(player::Player& player) {
	return isFreeForAll_.get() || allowedLooters->contains(player.getObjectId());
}

void DropNpc::setLootingPlayer(runtime::Ptr<player::Player> player) {
	this->lootingPlayer.set(player);
}

bool DropNpc::isBeingLooted() {
	return static_cast<bool>(lootingPlayer.get());
}

runtime::Ptr<team::common::legacy::LootGroupRules> DropNpc::getLootGroupRules() {
	runtime::Ptr<team::TemporaryPlayerTeam> team = lootingTeam.get(); // Java: lootingTeam.get() of a WeakReference (fieldmap.toml: a Ref)
	if (team)
		lastLootGroupRules.set(team->getLootGroupRules());
	return lastLootGroupRules.get();
}

void DropNpc::setLootingTeam(team::TemporaryPlayerTeam& team) {
	lootingTeam.set(runtime::Ptr<team::TemporaryPlayerTeam>(team));
	lootingTeamId.set(team.getTeamId());
	team::alliance::PlayerAlliance* alli = dynamic_cast<team::alliance::PlayerAlliance*>(&team);
	maxRoll.set(alli != nullptr ? alli->isInLeague() ? 10000 : 1000 : 100);
	lastLootGroupRules.set(team.getLootGroupRules());
}

void DropNpc::setInRangePlayers(runtime::Ptr<runtime::RcArrayList<runtime::Ref<player::Player>>> value) {
	inRangePlayers.set(value);
}

void DropNpc::addPlayerStatus(player::Player& player) {
	playerStatus.add(runtime::Ref<player::Player>(player));
}

void DropNpc::delPlayerStatus(player::Player& player) {
	playerStatus.remove(player);
}

bool DropNpc::containsPlayerStatus(player::Player& player) {
	return playerStatus.contains(player);
}

void DropNpc::startFreeForAll() {
	isFreeForAll_.set(true);
	distributionId.set(0);
	allowedLooters->clear();
}

DropNpc::~DropNpc() = default;

} // namespace aion::gameserver::model::gameobjects
