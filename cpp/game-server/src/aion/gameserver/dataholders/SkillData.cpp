#include "aion/gameserver/dataholders/SkillData.h"

#include <string>
#include <unordered_set>
#include <utility>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/dataholders/MotionData.h"
#include "aion/gameserver/dataholders/detail/JavaHashMapOrder.h"
#include "aion/gameserver/skillengine/model/Motion.h"

namespace aion::gameserver::dataholders {

using skillengine::model::SkillTemplate;

void SkillData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	skillTemplateById.clear();
	skillTemplatesByGroup.clear();
	skillTemplatesByStack.clear();
	std::vector<std::pair<int32_t, const SkillTemplate*>> puts;
	puts.reserve(skillTemplates.size());
	for (const SkillTemplate& skillTemplate : skillTemplates) {
		int32_t skillId = skillTemplate.getSkillId();
		skillTemplateById.insert_or_assign(skillId, &skillTemplate);
		puts.emplace_back(skillId, &skillTemplate);
		if (!skillTemplate.getGroup().empty()) // Java: getGroup() != null (absent attribute)
			skillTemplatesByGroup[skillTemplate.getGroup()].push_back(&skillTemplate);
		skillTemplatesByStack[skillTemplate.getStack()].push_back(&skillTemplate); // Java: getStack() != null, always (a required attribute)
	}
	skillTemplatesInHashOrder = detail::javaHashMapValues(puts);
	// Java: skillTemplates = null (the C++ indexes point into the storage, which stays)
}

const SkillTemplate* SkillData::getSkillTemplate(int32_t skillId) const {
	auto it = skillTemplateById.find(skillId);
	return it != skillTemplateById.end() ? it->second : nullptr;
}

const std::vector<const SkillTemplate*>* SkillData::getSkillTemplatesByStack(std::string_view skillStack) const {
	auto it = skillTemplatesByStack.find(skillStack);
	return it != skillTemplatesByStack.end() ? &it->second : nullptr;
}

int32_t SkillData::size() const {
	return static_cast<int32_t>(skillTemplateById.size());
}

const std::vector<const SkillTemplate*>* SkillData::getSkillTemplatesByGroup(std::string_view skillGroup) const {
	auto it = skillTemplatesByGroup.find(skillGroup);
	return it != skillTemplatesByGroup.end() ? &it->second : nullptr;
}

std::vector<const SkillTemplate*> SkillData::getSkillTemplates() const {
	return skillTemplatesInHashOrder;
}

void SkillData::validateMotions(const MotionData& motionData) const {
	std::string missing;
	std::unordered_set<std::string> motionNames;
	for (const SkillTemplate* t : getSkillTemplates()) {
		const skillengine::model::Motion* m = t->getMotion();
		if (m == nullptr || m->getName().empty()) // Java: m == null || m.getName() == null
			continue;
		if (motionNames.insert(m->getName()).second) {
			if (motionData.getMotionTime(m->getName()) == nullptr)
				missing.append("\"").append(m->getName()).append("\" (skill id ").append(std::to_string(t->getSkillId())).append("), ");
		}
	}
	if (!missing.empty())
		commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dataholders.SkillData")
		  .warn("Missing motion times for these motion names: {}", missing.substr(0, missing.size() - 2));
}

} // namespace aion::gameserver::dataholders
