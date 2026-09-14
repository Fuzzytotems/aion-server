#include "aion/gameserver/model/gameobjects/player/BlockList.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/player/BlockedPlayer.h"

namespace aion::gameserver::model::gameobjects::player {

BlockList::BlockList() = default;

BlockList::BlockList(const std::unordered_map<int32_t, runtime::Ptr<BlockedPlayer>>& initialList) {
	// Java: this.blockedList = new ConcurrentHashMap<>(initialList)
	for (const auto& [objId, blockedPlayer] : initialList)
		blockedList.put(objId, runtime::Ref<BlockedPlayer>(*blockedPlayer));
}

BlockList::~BlockList() = default;

runtime::Ref<BlockList> BlockList::create() {
	return runtime::makeRef<BlockList>();
}

runtime::Ref<BlockList> BlockList::create(const std::unordered_map<int32_t, runtime::Ptr<BlockedPlayer>>& initialList) {
	return runtime::makeRef<BlockList>(initialList);
}

void BlockList::add(BlockedPlayer& plr) {
	AION_UNPORTED();
}

void BlockList::remove(int32_t objIdOfPlayer) {
	AION_UNPORTED();
}

runtime::Ptr<BlockedPlayer> BlockList::getBlockedPlayer(std::string_view name) {
	AION_UNPORTED();
}

runtime::Ptr<BlockedPlayer> BlockList::getBlockedPlayer(int32_t playerObjId) {
	AION_UNPORTED();
}

bool BlockList::contains(int32_t playerObjectId) {
	AION_UNPORTED();
}

int32_t BlockList::getSize() {
	AION_UNPORTED();
}

bool BlockList::isFull() {
	AION_UNPORTED();
}

runtime::JavaIterator<runtime::Ptr<BlockedPlayer>> BlockList::iterator() {
	AION_UNPORTED();
}

runtime::SnapshotIterator<runtime::Ptr<BlockedPlayer>> BlockList::begin() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::gameobjects::player
