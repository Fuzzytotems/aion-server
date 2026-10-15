#include "aion/gameserver/dataholders/SkillTreeData.h"

#include <algorithm>

#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/model/PlayerClass.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/skillengine/model/SkillLearnTemplate.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"

namespace aion::gameserver::dataholders {

using skillengine::model::SkillLearnTemplate;
using skillengine::model::SkillTemplate;

void SkillTreeData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	for (const SkillLearnTemplate& template_ : skillTemplates) {
		if (!template_.getClassId()) {
			for (size_t ordinal = 0; ordinal < xml::EnumTraits<model::PlayerClass>::names.size(); ++ordinal)
				addTemplate(static_cast<model::PlayerClass>(ordinal), template_);
		} else {
			addTemplate(*template_.getClassId(), template_);
		}
	}
	// Java: skillTemplates = null (the C++ lists point into the storage, which stays)
}

void SkillTreeData::addTemplate(model::PlayerClass playerClass, const SkillLearnTemplate& template_) {
	int32_t hash = makeHash(xml::enumOrdinal(playerClass), xml::enumOrdinal(template_.getRace()), template_.getMinLevel());
	templates[hash].push_back(&template_);
	templatesById[template_.getSkillId()].push_back(&template_);
}

std::vector<const SkillLearnTemplate*> SkillTreeData::getTemplatesFor(model::PlayerClass playerClass, int32_t level, model::Race race) const {
	std::vector<const SkillLearnTemplate*> newSkills;
	auto classRaceSpecificTemplates = templates.find(makeHash(xml::enumOrdinal(playerClass), xml::enumOrdinal(race), level));
	auto classSpecificTemplates = templates.find(makeHash(xml::enumOrdinal(playerClass), xml::enumOrdinal(model::Race::PC_ALL), level));
	if (classRaceSpecificTemplates != templates.end())
		newSkills.insert(newSkills.end(), classRaceSpecificTemplates->second.begin(), classRaceSpecificTemplates->second.end());
	if (classSpecificTemplates != templates.end())
		newSkills.insert(newSkills.end(), classSpecificTemplates->second.begin(), classSpecificTemplates->second.end());
	return newSkills;
}

std::vector<const SkillLearnTemplate*> SkillTreeData::getSkillsForSkill(int32_t skillId, model::PlayerClass playerClass, model::Race race,
                                                                        int32_t playerLevel) const {
	std::vector<const SkillLearnTemplate*> skillTree;
	std::vector<const SkillLearnTemplate*> candidates = getTemplatesForSkill(getHighestSkill(skillId), playerClass, race);
	if (!candidates.empty())
		createSkillTree(candidates.front(), skillTree); // Java: the first template, then break
	if (playerLevel > -1)
		std::erase_if(skillTree, [playerLevel](const SkillLearnTemplate* template_) { return template_->getMinLevel() > playerLevel; });
	return skillTree;
}

void SkillTreeData::createSkillTree(const SkillLearnTemplate* topSkill, std::vector<const SkillLearnTemplate*>& addList) const {
	if (topSkill == nullptr)
		return;
	addList.insert(addList.begin(), topSkill);
	if (!topSkill->getLearnSkill())
		return;

	for (const SkillLearnTemplate* template_ : getTemplatesForSkill(*topSkill->getLearnSkill(), topSkill->getClassId(), topSkill->getRace())) {
		if (topSkill->isStigma() != template_->isStigma())
			continue;
		createSkillTree(template_, addList);
		break;
	}
}

std::vector<const SkillLearnTemplate*> SkillTreeData::getTemplatesForSkill(int32_t skillId, std::optional<model::PlayerClass> playerClass,
                                                                           model::Race race) const {
	std::vector<const SkillLearnTemplate*> searchSkills;
	auto byId = templatesById.find(skillId);
	if (byId != templatesById.end()) {
		for (const SkillLearnTemplate* template_ : byId->second) {
			if ((!template_->getClassId() || template_->getClassId() == playerClass) &&
			    (template_->getRace() == model::Race::PC_ALL || template_->getRace() == race))
				searchSkills.push_back(template_);
		}
	}
	return searchSkills;
}

int32_t SkillTreeData::getHighestSkill(int32_t skillId) {
	const SkillTemplate* baseTemplate = DataManager::SKILL_DATA->getSkillTemplate(skillId);
	if (baseTemplate == nullptr)
		return skillId;
	const std::vector<const SkillTemplate*>* stack = DataManager::SKILL_DATA->getSkillTemplatesByStack(baseTemplate->getStack());
	if (stack == nullptr)
		return skillId;
	// Java: stream().max(t1.getLvl() - t2.getLvl()).orElse(baseTemplate) - BinaryOperator.maxBy keeps the first of equal maxima
	const SkillTemplate* highestSkill = nullptr;
	for (const SkillTemplate* candidate : *stack) {
		if (highestSkill == nullptr || candidate->getLvl() - highestSkill->getLvl() > 0)
			highestSkill = candidate;
	}
	return (highestSkill != nullptr ? highestSkill : baseTemplate)->getSkillId();
}

bool SkillTreeData::isLearnedSkill(int32_t skillId) const {
	return templatesById.contains(skillId);
}

int32_t SkillTreeData::size() const {
	int32_t sum = 0;
	for (const auto& [hash, list] : templates)
		sum += static_cast<int32_t>(list.size());
	return sum;
}

int32_t SkillTreeData::makeHash(int32_t classId, int32_t race, int32_t level) {
	uint32_t result = static_cast<uint32_t>(classId) << 8;
	result = (result | static_cast<uint32_t>(race)) << 8;
	return static_cast<int32_t>(result | static_cast<uint32_t>(level));
}

} // namespace aion::gameserver::dataholders
