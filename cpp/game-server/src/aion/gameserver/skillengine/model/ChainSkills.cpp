#include "aion/gameserver/skillengine/model/ChainSkills.h"

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/skillengine/model/ChainSkill.h"

namespace aion::gameserver::skillengine::model {

ChainSkills::ChainSkills() : previousChainSkill(ChainSkill::create("")), chainSkill(ChainSkill::create("")) {
}

ChainSkills::~ChainSkills() = default;

runtime::Ref<ChainSkills> ChainSkills::create() {
	return runtime::makeRef<ChainSkills>();
}

int32_t ChainSkills::getCurrentChainCount(std::string_view category) {
	// Java: chainSkill.getCategory().equals(category) - a null category (Skill.chainCategory, "" here) never equals, and a ChainSkill whose
	// category is "" has never been used (its count is 0), so comparing "" with "" answers 0 as well
	return chainSkill->getCategory() == category ? chainSkill->getUseCount() : 0;
}

void ChainSkills::updateChain(std::string_view category, int32_t duration) {
	if (chainSkill->getCategory().empty())
		chainSkill->setCategory(category);
	else if (chainSkill->getCategory() != category) {
		previousChainSkill.set(chainSkill.get());
		chainSkill.set(ChainSkill::create(category));
	}
	chainSkill->increaseUseCount();
	expireTime.set(duration == 0 ? 0 : commons::utils::currentTimeMillis() + duration);
}

void ChainSkills::resetChain() {
	if (!chainSkill->getCategory().empty()) {
		expireTime.set(0);
		previousChainSkill->clear();
		chainSkill->clear();
	}
}

bool ChainSkills::isChainExpired() {
	return expireTime.get() > 0 && commons::utils::currentTimeMillis() > expireTime.get();
}

} // namespace aion::gameserver::skillengine::model
