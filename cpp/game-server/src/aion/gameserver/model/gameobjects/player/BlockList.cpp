#include "aion/gameserver/model/gameobjects/player/BlockList.h"

#include <memory>

#include "aion/commons/utils/StringUtils.h"
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
	blockedList.put(plr.getObjId(), runtime::Ref<BlockedPlayer>(plr));
}

void BlockList::remove(int32_t objIdOfPlayer) {
	blockedList.remove(objIdOfPlayer);
}

runtime::Ptr<BlockedPlayer> BlockList::getBlockedPlayer(std::string_view name) {
	for (const runtime::Ptr<BlockedPlayer>& entry : blockedList.values()) {
		if (commons::utils::StringUtils::equalsIgnoreCase(entry->getName(), name))
			return entry;
	}
	return nullptr;
}

runtime::Ptr<BlockedPlayer> BlockList::getBlockedPlayer(int32_t playerObjId) {
	return blockedList.get(playerObjId);
}

bool BlockList::contains(int32_t playerObjectId) {
	return blockedList.containsKey(playerObjectId);
}

int32_t BlockList::getSize() {
	return blockedList.size();
}

bool BlockList::isFull() {
	return getSize() >= MAX_BLOCKS;
}

runtime::JavaIterator<runtime::Ptr<BlockedPlayer>> BlockList::iterator() {
	return blockedList.values().iterator();
}

runtime::SnapshotIterator<runtime::Ptr<BlockedPlayer>> BlockList::begin() {
	return runtime::SnapshotIterator<runtime::Ptr<BlockedPlayer>>(
		std::make_shared<const std::vector<runtime::Ptr<BlockedPlayer>>>(blockedList.values().toVector()));
}

} // namespace aion::gameserver::model::gameobjects::player
