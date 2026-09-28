#include "aion/gameserver/model/craft/ProfessionInfo.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::model::craft {

// The Profession methods of header request m5c-h04 (m5c-plan.md I-02), declared in ProfessionInfo.h; their bodies are C-01's (P5-09c).

std::optional<int32_t> getUpgradeCost(Profession /*profession*/, int32_t /*skillLevel*/) {
	AION_UNPORTED();
}

int32_t getMaxUpgradableLevel(Profession /*profession*/) {
	AION_UNPORTED();
}

std::string getClientName(Profession /*profession*/) {
	AION_UNPORTED();
}

std::string getClientName(Profession /*profession*/, int32_t /*skillLevel*/) {
	AION_UNPORTED();
}

std::string getSkillGrade(Profession /*profession*/, int32_t /*skillLevel*/) {
	AION_UNPORTED();
}

std::optional<Profession> getBySkillId(int32_t /*skillId*/) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::craft
