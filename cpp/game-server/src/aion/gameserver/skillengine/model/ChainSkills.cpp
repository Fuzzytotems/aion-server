#include "aion/gameserver/skillengine/model/ChainSkills.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/skillengine/model/ChainSkill.h"

namespace aion::gameserver::skillengine::model {

ChainSkills::ChainSkills() : previousChainSkill(ChainSkill::create("")), chainSkill(ChainSkill::create("")) {
}

ChainSkills::~ChainSkills() = default;

runtime::Ref<ChainSkills> ChainSkills::create() {
	return runtime::makeRef<ChainSkills>();
}

int32_t ChainSkills::getCurrentChainCount(std::string_view category) {
	AION_UNPORTED();
}

void ChainSkills::updateChain(std::string_view category, int32_t duration) {
	AION_UNPORTED();
}

void ChainSkills::resetChain() {
	AION_UNPORTED();
}

bool ChainSkills::isChainExpired() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::skillengine::model
