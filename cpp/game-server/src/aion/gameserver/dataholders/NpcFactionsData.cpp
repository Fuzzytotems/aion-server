#include "aion/gameserver/dataholders/NpcFactionsData.h"

#include "aion/gameserver/dataholders/detail/JavaHashMapOrder.h"

namespace aion::gameserver::dataholders {

using model::templates::factions::NpcFactionTemplate;

void NpcFactionsData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	factionsById.clear();
	detail::JavaHashMapOrder<int32_t, const NpcFactionTemplate*> order;
	for (const NpcFactionTemplate& template_ : npcFactionsData) {
		factionsById.insert_or_assign(template_.getId(), &template_);
		order.put(template_.getId(), &template_, detail::javaHashCode(template_.getId()));
		if (template_.getNpcIds()) {
			for (int32_t npcId : *template_.getNpcIds())
				factionsByNpcId.insert_or_assign(npcId, &template_);
		}
	}
	factionsInHashOrder = order.values();
	// Java: npcFactionsData = null (the C++ indexes point into the storage, which stays)
}

const NpcFactionTemplate* NpcFactionsData::getNpcFactionById(int32_t id) const {
	auto it = factionsById.find(id);
	return it != factionsById.end() ? it->second : nullptr;
}

const NpcFactionTemplate* NpcFactionsData::getNpcFactionByNpcId(int32_t id) const {
	auto it = factionsByNpcId.find(id);
	return it != factionsByNpcId.end() ? it->second : nullptr;
}

const std::vector<const NpcFactionTemplate*>& NpcFactionsData::getNpcFactionsData() const {
	return factionsInHashOrder;
}

int32_t NpcFactionsData::size() const {
	return static_cast<int32_t>(factionsById.size());
}

} // namespace aion::gameserver::dataholders
