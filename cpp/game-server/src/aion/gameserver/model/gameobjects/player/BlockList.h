#pragma once

#include <cstdint>
#include <iterator>
#include <string_view>
#include <unordered_map>

#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/collections/Iterators.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"

namespace aion::gameserver::model::gameobjects::player {

/**
 * Represents a players list of blocked users<br />
 * Blocks via a player's CommonData
 * <p>
 * S0c declaration header (docs/design/hub-headers.md §3.5). RefCounted (fieldmap K4, `Player.blockList`), created with create(). Java
 * `Iterable<BlockedPlayer>`: iterator()/begin()/end() iterate a snapshot of the blocked players (§7.2).
 *
 * @author Ben
 */
class BlockList : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
public:
	static constexpr int32_t MAX_BLOCKS = 100;

private:
	// Indexes blocked players by their player ID
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<BlockedPlayer>> blockedList{AION_LOCK_CLASS(BlockList::blockedList#stripe)};

protected:
	/** Constructs a new (empty) blocked list */
	BlockList();

	/** Constructs a new blocked list with the given initial items */
	explicit BlockList(const std::unordered_map<int32_t, runtime::Ptr<BlockedPlayer>>& initialList);

	~BlockList() override;

public:
	/** Java: new BlockList() */
	static runtime::Ref<BlockList> create();

	/** Java: new BlockList(initialList) */
	static runtime::Ref<BlockList> create(const std::unordered_map<int32_t, runtime::Ptr<BlockedPlayer>>& initialList);

	/** Adds a player to the blocked users list<br /> Does not send packets or update the database */
	void add(BlockedPlayer& plr);

	/** Removes a player from the blocked users list<br /> Does not send packets or update the database */
	void remove(int32_t objIdOfPlayer);

	/** Returns the blocked player with this name if they exist, null otherwise */
	runtime::Ptr<BlockedPlayer> getBlockedPlayer(std::string_view name);

	runtime::Ptr<BlockedPlayer> getBlockedPlayer(int32_t playerObjId);

	bool contains(int32_t playerObjectId);

	/** Returns the number of blocked players in this list */
	int32_t getSize();

	bool isFull();

	/** Java: Iterator<BlockedPlayer> iterator() over blockedList.values() (§7.2) */
	runtime::JavaIterator<runtime::Ptr<BlockedPlayer>> iterator();

	/** C++ only: range-for over a snapshot of the blocked players (§7.2) */
	runtime::SnapshotIterator<runtime::Ptr<BlockedPlayer>> begin();

	std::default_sentinel_t end() const noexcept { return {}; }
};

} // namespace aion::gameserver::model::gameobjects::player
