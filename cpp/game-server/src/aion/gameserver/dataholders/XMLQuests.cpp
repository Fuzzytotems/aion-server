#include "aion/gameserver/dataholders/XMLQuests.h"

#include "aion/gameserver/dataholders/detail/JavaHashMapOrder.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::dataholders {

using questEngine::handlers::models::XMLQuest;

void XMLQuests::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	// Java: if (data != null) (an absent list is the empty bound vector and builds an empty index)
	questsById.clear();
	detail::JavaHashMapOrder<int32_t, const XMLQuest*> order;
	for (const std::unique_ptr<XMLQuest>& quest : data) {
		questsById.insert_or_assign(quest->getId(), quest.get());
		order.put(quest->getId(), quest.get(), detail::javaHashCode(quest->getId()));
	}
	questsInHashOrder = order.values();
	// Java: data = null (the C++ index points into the storage, which stays)
}

const std::vector<const XMLQuest*>& XMLQuests::getAllQuests() const {
	return questsInHashOrder;
}

const XMLQuest* XMLQuests::getQuest(int32_t questId) const {
	auto it = questsById.find(questId);
	return it != questsById.end() ? it->second : nullptr;
}

void XMLQuests::setData(std::vector<std::unique_ptr<XMLQuest>> /*data*/) {
	AION_UNPORTED(); // //reload of static data is deferred (design D3): a published holder is immutable
}

} // namespace aion::gameserver::dataholders
