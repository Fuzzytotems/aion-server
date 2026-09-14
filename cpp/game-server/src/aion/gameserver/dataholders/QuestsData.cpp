#include "aion/gameserver/dataholders/QuestsData.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::dataholders {

void QuestsData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	questTemplates.clear();
	sortedByFactionId.clear();
	for (const model::templates::QuestTemplate& quest : questsData) {
		questTemplates.insert_or_assign(quest.getId(), &quest);
		int32_t npcFactionId = quest.getNpcFactionId();
		if (npcFactionId == 0 || quest.isTimeBased())
			continue;
		sortedByFactionId[npcFactionId].push_back(&quest);
	}
	// Java: questsData = null (the C++ index points into the storage, which stays)
}

const model::templates::QuestTemplate* QuestsData::getQuestById(int32_t id) const {
	auto it = questTemplates.find(id);
	return it != questTemplates.end() ? it->second : nullptr;
}

std::vector<const model::templates::QuestTemplate*> QuestsData::getQuestsByNpcFaction(int32_t /*npcFactionId*/,
	model::gameobjects::player::Player& /*player*/) const {
	AION_UNPORTED();
}

int32_t QuestsData::size() const {
	return static_cast<int32_t>(questTemplates.size());
}

std::vector<const model::templates::QuestTemplate*> QuestsData::getQuestTemplates() const {
	// the port reproduces Java's HashMap<Integer, ...> iteration order
	AION_UNPORTED();
}

} // namespace aion::gameserver::dataholders
