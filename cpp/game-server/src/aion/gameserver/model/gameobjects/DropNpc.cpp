#include "aion/gameserver/model/gameobjects/DropNpc.h"

#include "aion/gameserver/runtime/base/Unported.h"
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
	AION_UNPORTED();
}

bool DropNpc::isAllowedToLoot(player::Player& player) {
	AION_UNPORTED();
}

void DropNpc::setLootingPlayer(runtime::Ptr<player::Player> player) {
	this->lootingPlayer.set(player);
}

bool DropNpc::isBeingLooted() {
	AION_UNPORTED();
}

runtime::Ptr<team::common::legacy::LootGroupRules> DropNpc::getLootGroupRules() {
	AION_UNPORTED();
}

void DropNpc::setLootingTeam(team::TemporaryPlayerTeam& team) {
	AION_UNPORTED();
}

void DropNpc::setInRangePlayers(runtime::Ptr<runtime::RcArrayList<runtime::Ref<player::Player>>> value) {
	inRangePlayers.set(value);
}

void DropNpc::addPlayerStatus(player::Player& player) {
	AION_UNPORTED();
}

void DropNpc::delPlayerStatus(player::Player& player) {
	AION_UNPORTED();
}

bool DropNpc::containsPlayerStatus(player::Player& player) {
	AION_UNPORTED();
}

void DropNpc::startFreeForAll() {
	AION_UNPORTED();
}

DropNpc::~DropNpc() = default;

} // namespace aion::gameserver::model::gameobjects
