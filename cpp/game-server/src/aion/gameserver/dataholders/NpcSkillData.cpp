#include "aion/gameserver/dataholders/NpcSkillData.h"

#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/dataholders/detail/JavaHashMapOrder.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::dataholders {

using model::templates::npcskill::NpcSkillTemplates;

void NpcSkillData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	detail::JavaHashMapOrder<int32_t, const NpcSkillTemplates*> order;
	for (const NpcSkillTemplates& npcSkillList : npcSkills) {
		if (!npcSkillList.getNpcIds())
			throw runtime::NullPointerException("NpcSkillTemplates.npcIds"); // Java: iterating the null list of a template without npc_ids
		for (int32_t npcId : *npcSkillList.getNpcIds()) {
			order.putIfAbsent(npcId, &npcSkillList, detail::javaHashCode(npcId));
			if (!npcSkillData.try_emplace(npcId, &npcSkillList).second)
				commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dataholders.NpcSkillData")
				  .warn("Npc " + std::to_string(npcId) + " has multiple skill lists in npc_skills.xml");
		}
	}
	skillListsInHashOrder = order.values();
	// Java: npcSkills = null (the C++ index points into the storage, which stays)
}

int32_t NpcSkillData::size() const {
	return static_cast<int32_t>(npcSkillData.size());
}

const NpcSkillTemplates* NpcSkillData::getNpcSkillList(int32_t id) const {
	auto it = npcSkillData.find(id);
	return it != npcSkillData.end() ? it->second : nullptr;
}

void NpcSkillData::setNpcSkillTemplates(std::vector<NpcSkillTemplates> /*template_*/) {
	AION_UNPORTED(); // //reload of static data is deferred (design D3): a published holder is immutable
}

const std::vector<const NpcSkillTemplates*>& NpcSkillData::getAllNpcSkillTemplates() const {
	return skillListsInHashOrder;
}

} // namespace aion::gameserver::dataholders
