#include "aion/gameserver/dataholders/ChestData.h"

namespace aion::gameserver::dataholders {

/** Initializes all maps for subsequent use - Don't nullify initial chest list as it will be used during reload */
void ChestData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	chestData.clear();
	for (const model::templates::chest::ChestTemplate& chest : chests)
		chestData.insert_or_assign(chest.getNpcId(), &chest);
	// Java: chests = null (the C++ index points into the storage, which stays)
}

int32_t ChestData::size() const {
	return static_cast<int32_t>(chestData.size());
}

const model::templates::chest::ChestTemplate* ChestData::getChestTemplate(int32_t npcId) const {
	auto it = chestData.find(npcId);
	return it != chestData.end() ? it->second : nullptr;
}

} // namespace aion::gameserver::dataholders
