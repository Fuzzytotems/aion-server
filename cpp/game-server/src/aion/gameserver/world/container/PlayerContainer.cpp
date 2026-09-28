#include "aion/gameserver/world/container/PlayerContainer.h"

#include <memory>
#include <string>

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/world/exceptions/DuplicateAionObjectException.h"

namespace aion::gameserver::world::container {

PlayerContainer::PlayerContainer() = default;

PlayerContainer::~PlayerContainer() = default;

runtime::Ref<PlayerContainer> PlayerContainer::create() {
	return runtime::makeRef<PlayerContainer>();
}

void PlayerContainer::add(model::gameobjects::player::Player& player) {
	if (playersById.put(player.getObjectId(), runtime::Ref<model::gameobjects::player::Player>(player)))
		throw exceptions::DuplicateAionObjectException(player, playersById.get(player.getObjectId()));
	if (playersByName.put(player.getName(), runtime::Ref<model::gameobjects::player::Player>(player)))
		throw exceptions::DuplicateAionObjectException(player, playersByName.get(player.getName()));
}

void PlayerContainer::remove(model::gameobjects::player::Player& player) {
	playersById.remove(player.getObjectId());
	playersByName.remove(player.getName());
}

runtime::Ptr<model::gameobjects::player::Player> PlayerContainer::get(int32_t objectId) {
	return playersById.get(objectId);
}

runtime::Ptr<model::gameobjects::player::Player> PlayerContainer::get(std::string_view name) {
	return playersByName.get(std::string(name));
}

runtime::JavaIterator<runtime::Ptr<model::gameobjects::player::Player>> PlayerContainer::iterator() {
	return playersById.values().iterator();
}

runtime::SnapshotIterator<runtime::Ptr<model::gameobjects::player::Player>> PlayerContainer::begin() {
	return runtime::SnapshotIterator<runtime::Ptr<model::gameobjects::player::Player>>(
		std::make_shared<const std::vector<runtime::Ptr<model::gameobjects::player::Player>>>(playersById.values().toVector()));
}

std::vector<runtime::Ptr<model::gameobjects::player::Player>> PlayerContainer::getAllPlayers() {
	// ensure there are no null values (due to concurrent object removal)
	std::vector<runtime::Ptr<model::gameobjects::player::Player>> players;
	for (runtime::Ptr<model::gameobjects::player::Player> p : playersById.values()) {
		if (p)
			players.push_back(p);
	}
	return players;
}

// C++ (D6, design 4.4): Java's compute(oldName) callback puts newName into the same map, which ConcurrentHashMap forbids and which can
// deadlock two stripe Monitors here. The put runs first and the old name is removed only while it still maps to this player.
void PlayerContainer::updateCachedPlayerName(std::string_view oldName, model::gameobjects::player::Player& player) {
	playersByName.put(player.getName(), runtime::Ref<model::gameobjects::player::Player>(player));
	playersByName.remove(std::string(oldName), runtime::Ref<model::gameobjects::player::Player>(player));
}

} // namespace aion::gameserver::world::container
