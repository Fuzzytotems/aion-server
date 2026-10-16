#include "aion/gameserver/dataholders/ChallengeData.h"

#include "aion/gameserver/dataholders/detail/JavaHashMapOrder.h"

namespace aion::gameserver::dataholders {

using model::templates::challenge::ChallengeQuestTemplate;
using model::templates::challenge::ChallengeTaskTemplate;

void ChallengeData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	detail::JavaHashMapOrder<int32_t, const ChallengeTaskTemplate*> order;
	for (const ChallengeTaskTemplate& t : task) {
		tasksById.insert_or_assign(t.getId(), &t);
		order.put(t.getId(), &t, detail::javaHashCode(t.getId()));
	}
	tasksInHashOrder = detail::toLinkedMap<decltype(tasksInHashOrder)>(order);
}

const detail::LinkedMap<int32_t, const ChallengeTaskTemplate*>& ChallengeData::getTasks() const {
	return tasksInHashOrder;
}

const ChallengeTaskTemplate* ChallengeData::getTaskByTaskId(int32_t taskId) const {
	auto it = tasksById.find(taskId);
	return it != tasksById.end() ? it->second : nullptr;
}

const ChallengeTaskTemplate* ChallengeData::getTaskByQuestId(int32_t questId) const {
	for (const auto& [id, ct] : tasksInHashOrder) {
		for (const ChallengeQuestTemplate& cq : ct->getQuests()) {
			if (cq.getId() == questId)
				return ct;
		}
	}
	return nullptr;
}

const ChallengeQuestTemplate* ChallengeData::getQuestByQuestId(int32_t questId) const {
	for (const auto& [id, ct] : tasksInHashOrder) {
		for (const ChallengeQuestTemplate& cq : ct->getQuests()) {
			if (cq.getId() == questId)
				return &cq;
		}
	}
	return nullptr;
}

int32_t ChallengeData::size() const {
	return static_cast<int32_t>(tasksById.size());
}

} // namespace aion::gameserver::dataholders
