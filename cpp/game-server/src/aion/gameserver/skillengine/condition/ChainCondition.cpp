#include "aion/gameserver/skillengine/condition/ChainCondition.h"

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/skillengine/model/ChainSkill.h"
#include "aion/gameserver/skillengine/model/ChainSkills.h"
#include "aion/gameserver/skillengine/model/Skill.h"

namespace aion::gameserver::skillengine::condition {

using gameserver::model::gameobjects::player::Player;
using runtime::Ptr;

// Java null for `category` and `preCategory` is the empty string here (xmlgen binds a missing attribute as ""); every <chain> of
// skill_templates.xml carries a non-empty category and none carries an empty precategory, so the two spellings never meet.

bool ChainCondition::validate(model::Skill& env) const {
	if (Ptr<Player> player = runtime::as<Player>(env.getEffector())) {
		Ptr<model::ChainSkills> chain = player->getChainSkills();
		Ptr<model::ChainSkill> currentSkill = chain->getCurrentChainSkill();
		if (shouldReset(*chain, env))
			chain->resetChain();
		if (!preCategory.empty()) {
			if (currentSkill->getCategory() == preCategory) {
				if (currentSkill->getUseCount() < preCount) // preCategory skill must have been activated x times
					return false;
			} else if (chain->getPreviousChainSkill()->getCategory() != preCategory) { // previously activated skill must match
				return false;
			}
		}
	}
	env.setChainCategory(category);
	env.setChainUsageDuration(time);
	return true;
}

bool ChainCondition::shouldReset(model::ChainSkills& chain, model::Skill& env) const {
	Ptr<model::ChainSkill> currentSkill = chain.getCurrentChainSkill();
	if (!currentSkill->getCategory().empty()) {
		if (chain.isChainExpired()) // check max allowed use time
			return true;
		if (preCategory.empty() && category.find("_1TH") != std::string::npos) { // first skill of a chain
			if (currentSkill->getCategory() != category) // other skill
				return true;
			if (currentSkill->getUseCount() == selfCount) // same skill
				return true;
			int32_t maxActiveDuration = time > 0 ? time : env.getCooldown() * 100; // template cooldown is seconds * 10...
			if (commons::utils::currentTimeMillis() > currentSkill->getLastUseTime() + maxActiveDuration)
				return true;
		}
	}
	return false;
}

} // namespace aion::gameserver::skillengine::condition
