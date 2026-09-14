#include "aion/gameserver/services/craft/CraftSkillUpdateService.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::services::craft {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.craft.CraftSkillUpdateService");

CraftSkillUpdateService::CraftSkillUpdateService() {
	AION_UNPORTED();
}

CraftSkillUpdateService::~CraftSkillUpdateService() = default;

CraftSkillUpdateService& CraftSkillUpdateService::getInstance() {
	static CraftSkillUpdateService instance; // Java SingletonHolder
	return instance;
}

std::optional<model::craft::Profession> CraftSkillUpdateService::getProfessionByNpc(model::gameobjects::Npc& npc) {
	AION_UNPORTED();
}

// anonymous RequestResponseHandler at CraftSkillUpdateService.java:108 (fieldmap key CraftSkillUpdateService$1); local responseHandler; storage: stored in ResponseRequester
void CraftSkillUpdateService::learnSkill(model::gameobjects::player::Player& player, model::gameobjects::Npc& npc) {
	AION_UNPORTED();
}

int32_t CraftSkillUpdateService::getTotalExpertCraftingSkills(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

int32_t CraftSkillUpdateService::getTotalMasterCraftingSkills(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

bool CraftSkillUpdateService::canLearnMoreExpertCraftingSkill(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

bool CraftSkillUpdateService::canLearnMoreMasterCraftingSkill(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services::craft
