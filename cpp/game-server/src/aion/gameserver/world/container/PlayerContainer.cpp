#include "aion/gameserver/world/container/PlayerContainer.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"

namespace aion::gameserver::world::container {

PlayerContainer::PlayerContainer() = default;

PlayerContainer::~PlayerContainer() = default;

runtime::Ref<PlayerContainer> PlayerContainer::create() {
	return runtime::makeRef<PlayerContainer>();
}

void PlayerContainer::add(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void PlayerContainer::remove(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

runtime::Ptr<model::gameobjects::player::Player> PlayerContainer::get(int32_t objectId) {
	AION_UNPORTED();
}

runtime::Ptr<model::gameobjects::player::Player> PlayerContainer::get(std::string_view name) {
	AION_UNPORTED();
}

runtime::JavaIterator<runtime::Ptr<model::gameobjects::player::Player>> PlayerContainer::iterator() {
	AION_UNPORTED();
}

runtime::SnapshotIterator<runtime::Ptr<model::gameobjects::player::Player>> PlayerContainer::begin() {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<model::gameobjects::player::Player>> PlayerContainer::getAllPlayers() {
	AION_UNPORTED();
}

// callbacks: com.aionemu.gameserver.world.container.PlayerContainer@L51 (compute callback on playersByName, invoked during the call)
void PlayerContainer::updateCachedPlayerName(std::string_view oldName, model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::world::container
