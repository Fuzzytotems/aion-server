#pragma once

#include <cstdint>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/collections/Iterators.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/world/container/fwd.h"

namespace aion::gameserver::world::container {

/**
 * Container for storing Players by objectId and name.
 * <p>
 * S0c declaration header (docs/design/hub-headers.md §3.5). RefCounted (fieldmap K4, `World::allPlayers`), created with create().
 * Java `Iterable<Player>`: iterator()/begin()/end() iterate a snapshot of playersById's values (§7.2). updateCachedPlayerName's compute
 * callback writes another key of the same map (Java's per-bin locks; the kernel's stripe Monitors are reentrant, ConcurrentHashMap.h).
 *
 * @author -Nemesiss-, Neon
 */
class PlayerContainer : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<model::gameobjects::player::Player>> playersById{
		AION_LOCK_CLASS(PlayerContainer::playersById#stripe)};
	runtime::ConcurrentHashMap<std::string, runtime::Ref<model::gameobjects::player::Player>> playersByName{
		AION_LOCK_CLASS(PlayerContainer::playersByName#stripe)};

protected:
	PlayerContainer();
	~PlayerContainer() override;

public:
	/** Java: new PlayerContainer() */
	static runtime::Ref<PlayerContainer> create();

	void add(model::gameobjects::player::Player& player);

	void remove(model::gameobjects::player::Player& player);

	runtime::Ptr<model::gameobjects::player::Player> get(int32_t objectId);

	runtime::Ptr<model::gameobjects::player::Player> get(std::string_view name);

	/** Java: Iterator<Player> iterator() over playersById.values() (§7.2) */
	runtime::JavaIterator<runtime::Ptr<model::gameobjects::player::Player>> iterator();

	/** C++ only: range-for over a snapshot of the players (§7.2) */
	runtime::SnapshotIterator<runtime::Ptr<model::gameobjects::player::Player>> begin();

	std::default_sentinel_t end() const noexcept { return {}; }

	/** Java: Collection<Player> (a new list) */
	std::vector<runtime::Ptr<model::gameobjects::player::Player>> getAllPlayers();

	void updateCachedPlayerName(std::string_view oldName, model::gameobjects::player::Player& player);
};

} // namespace aion::gameserver::world::container
