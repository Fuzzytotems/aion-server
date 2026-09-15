#include "aion/gameserver/model/templates/QuestTemplate.h"

#include <algorithm>

#include "aion/gameserver/configs/main/CraftConfig.h"

namespace aion::gameserver::model::templates {

int32_t QuestTemplate::getRequiredConditionCount() const {
	if (getXMLStartConditions().empty())
		return 0;
	int32_t optionalCount = 0;
	int32_t mandatoryCount = 0;
	for (const quest::XMLStartCondition& cond : startConds) {
		if (cond.isOptional())
			optionalCount++;
		else
			mandatoryCount++;
	}

	int32_t result = std::min(1, optionalCount) + mandatoryCount;

	if (isMaster())
		result += 1 - configs::main::CraftConfig::MAX_MASTER_CRAFTING_SKILLS.load();

	return result;
}

const std::vector<PlayerClass>& QuestTemplate::getClassPermitted() const {
	static const std::vector<PlayerClass> empty;
	return classPermitted ? *classPermitted : empty;
}

const std::vector<quest::QuestItems>& QuestTemplate::getSelectableRewardByClass(PlayerClass playerClass) const {
	static const std::vector<quest::QuestItems> empty;
	switch (playerClass) {
		case PlayerClass::ASSASSIN:
			return assassinSelectableReward;
		case PlayerClass::CHANTER:
			return chanterSelectableReward;
		case PlayerClass::CLERIC:
			return priestSelectableReward;
		case PlayerClass::GLADIATOR:
			return fighterSelectableReward;
		case PlayerClass::RANGER:
			return rangerSelectableReward;
		case PlayerClass::SORCERER:
			return wizardSelectableReward;
		case PlayerClass::SPIRIT_MASTER:
			return elementalistSelectableReward;
		case PlayerClass::TEMPLAR:
			return knightSelectableReward;
		case PlayerClass::GUNNER:
			return gunnerSelectableReward;
		case PlayerClass::BARD:
			return bardSelectableReward;
		case PlayerClass::RIDER:
			return riderSelectableReward;
		default:
			break;
	}
	return empty;
}

bool QuestTemplate::isDaily() const {
	return isTimeBased() && std::ranges::find(*repeatCycle, quest::QuestRepeatCycle::ALL) != repeatCycle->end();
}

} // namespace aion::gameserver::model::templates
