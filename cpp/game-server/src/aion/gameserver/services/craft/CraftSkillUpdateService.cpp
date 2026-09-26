#include "aion/gameserver/services/craft/CraftSkillUpdateService.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/model/craft/ProfessionInfo.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::services::craft {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.craft.CraftSkillUpdateService");

CraftSkillUpdateService::CraftSkillUpdateService() {
	using model::craft::Profession;
	// Asmodian
	professionByNpc.put(204096, Profession::ESSENCETAPPING);
	professionByNpc.put(830150, Profession::ESSENCETAPPING);
	professionByNpc.put(204257, Profession::AETHERTAPPING);
	professionByNpc.put(830148, Profession::AETHERTAPPING);

	professionByNpc.put(204100, Profession::COOKING);
	professionByNpc.put(830142, Profession::COOKING);
	professionByNpc.put(204104, Profession::WEAPONSMITHING);
	professionByNpc.put(830146, Profession::WEAPONSMITHING);
	professionByNpc.put(204106, Profession::ARMORSMITHING);
	professionByNpc.put(830144, Profession::ARMORSMITHING);
	professionByNpc.put(204110, Profession::TAILORING);
	professionByNpc.put(830136, Profession::TAILORING);
	professionByNpc.put(204102, Profession::ALCHEMY);
	professionByNpc.put(830138, Profession::ALCHEMY);
	professionByNpc.put(204108, Profession::HANDICRAFTING);
	professionByNpc.put(830140, Profession::HANDICRAFTING);
	professionByNpc.put(798452, Profession::CONSTRUCTION);
	professionByNpc.put(798456, Profession::CONSTRUCTION);

	// Elyos
	professionByNpc.put(203780, Profession::ESSENCETAPPING);
	professionByNpc.put(830066, Profession::ESSENCETAPPING);
	professionByNpc.put(203782, Profession::AETHERTAPPING);
	professionByNpc.put(830064, Profession::AETHERTAPPING);

	professionByNpc.put(203784, Profession::COOKING);
	professionByNpc.put(830058, Profession::COOKING);
	professionByNpc.put(203788, Profession::WEAPONSMITHING);
	professionByNpc.put(830062, Profession::WEAPONSMITHING);
	professionByNpc.put(203790, Profession::ARMORSMITHING);
	professionByNpc.put(830060, Profession::ARMORSMITHING);
	professionByNpc.put(203793, Profession::TAILORING);
	professionByNpc.put(830052, Profession::TAILORING);
	professionByNpc.put(203786, Profession::ALCHEMY);
	professionByNpc.put(830054, Profession::ALCHEMY);
	professionByNpc.put(203792, Profession::HANDICRAFTING);
	professionByNpc.put(830056, Profession::HANDICRAFTING);
	professionByNpc.put(798450, Profession::CONSTRUCTION);
	professionByNpc.put(798454, Profession::CONSTRUCTION);

	log.info("CraftSkillUpdateService: Initialized.");
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
	int32_t mastered = 0;

	for (model::craft::Profession profession : model::craft::PROFESSION_VALUES) {
		if (model::craft::isCrafting(profession) && player.getSkillList()->isSkillPresent(model::craft::getSkillId(profession))) {
			int32_t skillLvl = player.getSkillList()->getSkillLevel(model::craft::getSkillId(profession));
			if (skillLvl > 399 && skillLvl <= 499)
				mastered++;
		}
	}
	return mastered;
}

int32_t CraftSkillUpdateService::getTotalMasterCraftingSkills(model::gameobjects::player::Player& player) {
	int32_t mastered = 0;

	for (model::craft::Profession profession : model::craft::PROFESSION_VALUES) {
		if (model::craft::isCrafting(profession) && player.getSkillList()->isSkillPresent(model::craft::getSkillId(profession))) {
			int32_t skillLvl = player.getSkillList()->getSkillLevel(model::craft::getSkillId(profession));
			if (skillLvl > 499)
				mastered++;
		}
	}

	return mastered;
}

bool CraftSkillUpdateService::canLearnMoreExpertCraftingSkill(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

bool CraftSkillUpdateService::canLearnMoreMasterCraftingSkill(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services::craft
