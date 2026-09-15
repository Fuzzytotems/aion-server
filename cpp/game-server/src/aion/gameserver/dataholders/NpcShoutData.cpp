#include "aion/gameserver/dataholders/NpcShoutData.h"

#include <algorithm>

namespace aion::gameserver::dataholders {

using model::templates::npcshout::NpcShout;
using model::templates::npcshout::ShoutEventType;
using model::templates::npcshout::ShoutGroup;
using model::templates::npcshout::ShoutList;
using ShoutVector = std::vector<const NpcShout*>;

void NpcShoutData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	for (const ShoutGroup& group : shoutGroups) {
		const std::vector<ShoutList>& shoutNpcs = group.getShoutNpcs();
		for (int32_t i = static_cast<int32_t>(shoutNpcs.size()) - 1; i >= 0; i--) {
			const ShoutList& shoutList = shoutNpcs[static_cast<size_t>(i)];
			int32_t worldId = shoutList.getRestrictWorld();
			auto& worldShouts = shoutsByWorldNpcs[worldId];

			this->count += static_cast<int32_t>(shoutList.getNpcShouts().size());
			const std::vector<int32_t>& npcIds = shoutList.getNpcIds();
			for (int32_t j = static_cast<int32_t>(npcIds.size()) - 1; j >= 0; j--) {
				int32_t npcId = npcIds[static_cast<size_t>(j)];
				ShoutVector& shouts = worldShouts[npcId]; // Java: put a new list or addAll to the present one
				for (const NpcShout& shout : shoutList.getNpcShouts())
					shouts.push_back(&shout);
				// Java: shoutList.getNpcIds().remove(j)
			}
			// Java: shoutList.getNpcShouts().clear(); shoutList.makeNull(); group.getShoutNpcs().remove(i) (the C++ lists stay)
		}
		// Java: group.makeNull()
	}
	// Java: this.shoutGroups.clear(); this.shoutGroups = null (the C++ index points into the storage, which stays)
}

int32_t NpcShoutData::size() const {
	return this->count;
}

std::optional<ShoutVector> NpcShoutData::getNpcShouts(int32_t worldId, int32_t npcId) const {
	auto globalShouts = shoutsByWorldNpcs.find(0);
	auto worldShouts = shoutsByWorldNpcs.find(worldId);
	const ShoutVector* globalNpcShouts = nullptr;
	const ShoutVector* worldNpcShouts = nullptr;
	if (globalShouts != shoutsByWorldNpcs.end()) {
		auto it = globalShouts->second.find(npcId);
		if (it != globalShouts->second.end())
			globalNpcShouts = &it->second;
	}
	if (worldShouts != shoutsByWorldNpcs.end()) {
		auto it = worldShouts->second.find(npcId);
		if (it != worldShouts->second.end())
			worldNpcShouts = &it->second;
	}

	if (globalShouts == shoutsByWorldNpcs.end() && worldShouts == shoutsByWorldNpcs.end())
		return std::nullopt;

	ShoutVector npcShouts;
	if (globalNpcShouts != nullptr)
		npcShouts.insert(npcShouts.end(), globalNpcShouts->begin(), globalNpcShouts->end());
	// Java: the same list twice for worldId 0 (globalShouts == worldShouts)
	if (worldNpcShouts != nullptr)
		npcShouts.insert(npcShouts.end(), worldNpcShouts->begin(), worldNpcShouts->end());
	return npcShouts;
}

std::optional<ShoutVector> NpcShoutData::getNpcShouts(int32_t worldId, int32_t npcId, std::optional<ShoutEventType> type) const {
	std::optional<ShoutVector> shouts = getNpcShouts(worldId, npcId);
	if (!shouts || !type)
		return shouts;
	ShoutVector filtered;
	for (const NpcShout* shout : *shouts) {
		if (shout->getWhen() == *type)
			filtered.push_back(shout);
	}
	return filtered;
}

std::optional<ShoutVector> NpcShoutData::getNpcShouts(int32_t worldId, int32_t npcId, std::optional<ShoutEventType> type,
                                                      std::optional<std::string_view> pattern, int32_t skillNo) const {
	std::optional<ShoutVector> shouts = getNpcShouts(worldId, npcId, type);
	if (!shouts)
		return shouts;
	std::erase_if(
	  *shouts, [&](const NpcShout* shout) { return (pattern && *pattern != shout->getPattern()) || (skillNo != 0 && skillNo != shout->getSkillNo()); });
	return shouts;
}

} // namespace aion::gameserver::dataholders
