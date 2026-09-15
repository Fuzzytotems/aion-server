#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "aion/gameserver/dataholders/SkillData.xml.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.SkillData.
 * <p>
 * C++: the @XmlTransient index maps point into the bound `skillTemplates` storage, which stays after afterUnmarshal (static-data.md §2.6; Java
 * sets the list to null). A template without a group (absent attribute, Java null) has an empty group and is not indexed by group, like Java
 * (static-data.md §2.4: no skill template has a present empty group). Java's public lookups are declared (header requests pre-1 and
 * shells-1).
 *
 * @author ATracer, Neon
 */
class SkillData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/SkillData.xml.inc"
private:
	std::unordered_map<int32_t, const skillengine::model::SkillTemplate*> skillTemplateById;
	std::map<std::string, std::vector<const skillengine::model::SkillTemplate*>, std::less<>> skillTemplatesByGroup;
	std::map<std::string, std::vector<const skillengine::model::SkillTemplate*>, std::less<>> skillTemplatesByStack;
	/** C++ only: skillTemplateById.values() in Java's HashMap iteration order, computed by afterUnmarshal */
	std::vector<const skillengine::model::SkillTemplate*> skillTemplatesInHashOrder;

public:
	/** @return the skill template, nullptr (Java null) for an unknown id */
	const skillengine::model::SkillTemplate* getSkillTemplate(int32_t skillId) const;

	/** @return All skill templates of this stack, nullptr (Java null) if there is none */
	const std::vector<const skillengine::model::SkillTemplate*>* getSkillTemplatesByStack(std::string_view skillStack) const;

	/** @return Number of loaded skill templates */
	int32_t size() const;

	/**
	 * @return All skill templates of this group, nullptr (Java null) if there is none. A group is less precise and may be null.
	 */
	const std::vector<const skillengine::model::SkillTemplate*>* getSkillTemplatesByGroup(std::string_view skillGroup) const;

	/** Java `skillTemplateById.values()`: a snapshot in Java's HashMap<Integer, SkillTemplate> iteration order */
	std::vector<const skillengine::model::SkillTemplate*> getSkillTemplates() const;
};

} // namespace aion::gameserver::dataholders
