#include "aion/gameserver/skillengine/effect/PetOrderUseUltraSkillEffect.h"

#include <cstdint>
#include <optional>
#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/PetSkillData.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Summon.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SUMMON_USESKILL.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::skillengine::effect {

void PetOrderUseUltraSkillEffect::applyEffect(model::Effect& effect) const {
	gameserver::model::gameobjects::player::Player& effector = *runtime::cast<gameserver::model::gameobjects::player::Player>(effect.getEffector());

	if (!effector.getSummon() || effector.getSummon()->isBeingReleased()) {
		return;
	}

	int32_t effectorId = effector.getSummon()->getObjectId();

	int32_t npcId = effector.getSummon()->getNpcId();
	int32_t orderSkillId = effect.getSkillId();

	int32_t petUseSkillId = dataholders::DataManager::PET_SKILL_DATA->getPetOrderSkill(orderSkillId, npcId);
	const model::SkillTemplate* skillTemplate = dataholders::DataManager::SKILL_DATA->getSkillTemplate(petUseSkillId);
	if (skillTemplate == nullptr) {
		commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.skillengine.effect.PetOrderUseUltraSkillEffect")
			.warn("Couldn't find summon skill template for ID " + std::to_string(petUseSkillId) + " (summon order skill ID " +
				std::to_string(orderSkillId) + ")");
		return;
	}
	int32_t skillLvl = skillTemplate->getLvl();
	int32_t targetId = effect.getEffected()->getObjectId();
	int32_t hate = effect.getEffectHate() > 1 ? effect.getEffectHate() : 0;
	effector.getSummon()->addSkillOrder(petUseSkillId, skillLvl, *effect.getEffected(), hate, release);
	utils::PacketSendUtility::sendPacket(effector, network::aion::serverpackets::SM_SUMMON_USESKILL(effectorId, petUseSkillId, skillLvl, targetId));
}

void PetOrderUseUltraSkillEffect::calculate(model::Effect& effect) const {
	if (runtime::as<gameserver::model::gameobjects::player::Player>(effect.getEffector()) && effect.getEffected())
		EffectTemplate::calculate(effect, std::nullopt, std::nullopt);
}

} // namespace aion::gameserver::skillengine::effect
