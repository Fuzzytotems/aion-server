#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "aion/gameserver/dataholders/SkillTreeData.xml.h"
#include "aion/gameserver/model/fwd.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.SkillTreeData.
 * <p>
 * C++: the lists point into the bound `skillTemplates` storage, which stays after afterUnmarshal (static-data.md §2.6). getHighestSkill reads
 * the published DataManager::SKILL_DATA like Java.
 *
 * @author ATracer
 */
class SkillTreeData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/SkillTreeData.xml.inc"
private:
	std::unordered_map<int32_t, std::vector<const skillengine::model::SkillLearnTemplate*>> templates;
	std::unordered_map<int32_t, std::vector<const skillengine::model::SkillLearnTemplate*>> templatesById;

	void addTemplate(model::PlayerClass playerClass, const skillengine::model::SkillLearnTemplate& template_);

public:
	/**
	 * Perform search for all skill templates that match the given class, level and race.
	 */
	std::vector<const skillengine::model::SkillLearnTemplate*> getTemplatesFor(model::PlayerClass playerClass, int32_t level, model::Race race) const;

	/**
	 * @return All skills that are of the same skill stack for this skill ID. Class and race are necessary but playerLevel can be set to -1 if you
	 *         want the full skill list. It's needed since every level of a certain skill has its own ID since 4.8. If you would only add the
	 *         current skill ID, you wouldn't be able to use the lower level versions of a skill.
	 */
	std::vector<const skillengine::model::SkillLearnTemplate*> getSkillsForSkill(int32_t skillId, model::PlayerClass playerClass, model::Race race,
	                                                                             int32_t playerLevel) const;

private:
	/**
	 * Creates skill tree list in ascending player level order recursively
	 *
	 * @param topSkill
	 *          - max/best skill version of the stack
	 * @param addList
	 *          - initially an empty list to which skills are added
	 */
	void createSkillTree(const skillengine::model::SkillLearnTemplate* topSkill,
	                     std::vector<const skillengine::model::SkillLearnTemplate*>& addList) const;

public:
	/**
	 * @return All skill learn templates with the specified skill ID, that match the players class and race. Should return 1 template max. If more,
	 *         most probably skill_tree.xml is parsed incorrectly.
	 */
	std::vector<const skillengine::model::SkillLearnTemplate*> getTemplatesForSkill(int32_t skillId, std::optional<model::PlayerClass> playerClass,
	                                                                                model::Race race) const;

private:
	/** @return The skill id with the highest skill level of this skills' stack. */
	static int32_t getHighestSkill(int32_t skillId);

public:
	bool isLearnedSkill(int32_t skillId) const;

	int32_t size() const;

private:
	static int32_t makeHash(int32_t classId, int32_t race, int32_t level);
};

} // namespace aion::gameserver::dataholders
