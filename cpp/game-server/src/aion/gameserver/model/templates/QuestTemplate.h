#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/model/templates/QuestTemplate.xml.h"

#include "aion/gameserver/model/templates/L10n.h"

namespace aion::gameserver::model::templates {

/**
 * Java com.aionemu.gameserver.model.templates.QuestTemplate.
 * <p>
 * C++: the list getters return the bound lists; an absent list is empty, which is what Java's `Collections.emptyList()` gives callers.
 *
 * @author MrPoke, vlog, Neon
 */
class QuestTemplate : public ::aion::gameserver::runtime::StaticTemplate, public ::aion::gameserver::model::templates::L10n {
#include "aion/gameserver/model/templates/QuestTemplate.xml.inc"
public:
	const std::vector<quest::Rewards>& getRewards() const { return rewards; }

	/** Java returns Collections.emptyList() for a quest without drops; the C++ list always exists */
	const std::vector<quest::QuestDrop>& getQuestDrop() const { return questDrop; }

	const std::vector<quest::QuestKill>& getQuestKill() const { return questKill; }

	const std::vector<quest::XMLStartCondition>& getXMLStartConditions() const { return startConds; }

	int32_t getRequiredConditionCount() const;

	/** Java returns Collections.emptyList() without class_permitted */
	const std::vector<PlayerClass>& getClassPermitted() const;

	const std::vector<quest::QuestItems>& getSelectableRewardByClass(PlayerClass playerClass) const;

	int32_t getL10nId() const override { return nameId; }

	bool isClassRewardOnEveryRepeat() const { return useClassReward == 1; }

	bool isSingleTimeClassReward() const { return useClassReward == 2; }

	bool isRepeatable() const { return getMaxRepeatCount() > 1; }

	bool isMentor() const { return mentorType != quest::QuestMentorType::NONE; }

	bool isTimeBased() const { return repeatCycle.has_value(); }

	bool isDaily() const;

	bool isWeekly() const { return isTimeBased() && !isDaily(); }

	bool isMaster() const { return getCombineSkillPoint() == 499; }

	bool isExpert() const { return getCombineSkillPoint() == 399; }

	bool isProfession() const { return isMaster() || isExpert(); }

	/** @return True, if the quest is a mission quest (campaign quest) */
	bool isMission() const { return category == quest::QuestCategory::MISSION; }

	bool isNoCount() const { return category == quest::QuestCategory::NON_COUNT || category == quest::QuestCategory::EVENT; }
};

} // namespace aion::gameserver::model::templates
