#include "aion/gameserver/dataholders/NpcSkillData.h"

#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::dataholders {

void NpcSkillData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	for (const model::templates::npcskill::NpcSkillTemplates& npcSkillList : npcSkills) {
		if (!npcSkillList.getNpcIds())
			throw runtime::NullPointerException("NpcSkillTemplates.npcIds"); // Java: iterating the null list of a template without npc_ids
		for (int32_t npcId : *npcSkillList.getNpcIds()) {
			if (!npcSkillData.try_emplace(npcId, &npcSkillList).second)
				commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dataholders.NpcSkillData")
					.warn("Npc " + std::to_string(npcId) + " has multiple skill lists in npc_skills.xml");
		}
	}
	// Java: npcSkills = null (the C++ index points into the storage, which stays)
}

int32_t NpcSkillData::size() const {
	return static_cast<int32_t>(npcSkillData.size());
}

const model::templates::npcskill::NpcSkillTemplates* NpcSkillData::getNpcSkillList(int32_t id) const {
	auto it = npcSkillData.find(id);
	return it != npcSkillData.end() ? it->second : nullptr;
}

} // namespace aion::gameserver::dataholders
