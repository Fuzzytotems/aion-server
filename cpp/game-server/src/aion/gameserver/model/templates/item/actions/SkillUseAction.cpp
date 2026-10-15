#include "aion/gameserver/model/templates/item/actions/SkillUseAction.h"

#include <memory>
#include <vector>

#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/effect/EffectTemplate.h"
#include "aion/gameserver/skillengine/effect/Effects.h"
#include "aion/gameserver/skillengine/effect/HealEffect.h"
#include "aion/gameserver/skillengine/effect/HealInstantEffect.h"
#include "aion/gameserver/skillengine/effect/MPHealEffect.h"
#include "aion/gameserver/skillengine/effect/MPHealInstantEffect.h"
#include "aion/gameserver/skillengine/effect/ProcHealInstantEffect.h"
#include "aion/gameserver/skillengine/effect/ProcMPHealInstantEffect.h"
#include "aion/gameserver/skillengine/effect/SummonEffect.h"
#include "aion/gameserver/skillengine/effect/TransformEffect.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/skillengine/properties/Properties_CastState.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::model::templates::item::actions {

namespace {

using gameobjects::Creature;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using runtime::Ptr;
using runtime::Ref;
using skillengine::effect::EffectTemplate;
using utils::PacketSendUtility;

/** Java `skill.getSkillTemplate().getEffects().getEffects()`: a skill template without <effects> is a NullPointerException */
const std::vector<std::unique_ptr<EffectTemplate>>& effectsOf(skillengine::model::Skill& skill) {
	const skillengine::effect::Effects* effects = skill.getSkillTemplate()->getEffects();
	if (effects == nullptr)
		throw runtime::NullPointerException("effects");
	return effects->getEffects();
}

/** Java `template instanceof T` */
template <class T>
bool isInstance(const EffectTemplate& effectTemplate) {
	return dynamic_cast<const T*>(&effectTemplate) != nullptr;
}

/**
 * Java: the private static SkillUseAction.isIneffectiveHealSkill(List<EffectTemplate>, List<Creature>) (SkillUseAction.java:82-97). A
 * file-local function until header request m5b3-h04 declares it (docs/porting/header-requests.md; the m5b2-s2-2 workaround); canAct passes the
 * effected list as the snapshot the request names.
 */
bool isIneffectiveHealSkill(const std::vector<std::unique_ptr<EffectTemplate>>& effects, const std::vector<Ptr<Creature>>& effectedList) {
	int32_t hpHealEffects = 0, mpHealEffects = 0;
	for (const std::unique_ptr<EffectTemplate>& effectTemplate : effects) {
		if (effectTemplate->getValue() < 0) // negative heal value means damage
			return false;
		if (isInstance<skillengine::effect::HealEffect>(*effectTemplate) || isInstance<skillengine::effect::HealInstantEffect>(*effectTemplate)
			|| isInstance<skillengine::effect::ProcHealInstantEffect>(*effectTemplate)) {
			hpHealEffects++;
		} else if (isInstance<skillengine::effect::MPHealEffect>(*effectTemplate) || isInstance<skillengine::effect::MPHealInstantEffect>(*effectTemplate)
			|| isInstance<skillengine::effect::ProcMPHealInstantEffect>(*effectTemplate)) {
			mpHealEffects++;
		} else {
			return false;
		}
	}
	auto allFullyRestoredHp = [&effectedList] {
		for (const Ptr<Creature>& effected : effectedList) {
			if (!effected->getLifeStats()->isFullyRestoredHp())
				return false;
		}
		return true;
	};
	auto allFullyRestoredMp = [&effectedList] {
		for (const Ptr<Creature>& effected : effectedList) {
			if (!effected->getLifeStats()->isFullyRestoredMp())
				return false;
		}
		return true;
	};
	return (hpHealEffects == 0 || allFullyRestoredHp()) && (mpHealEffects == 0 || allFullyRestoredMp());
}

} // namespace

bool SkillUseAction::canAct(gameobjects::player::Player& player, runtime::Ptr<gameobjects::Item> parentItem,
	runtime::Ptr<gameobjects::Item> /*targetItem*/, std::initializer_list<std::any> /*params*/) const {
	if (mapid != 0 && player.getWorldId() != mapid) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_SKILL_CAN_NOT_USE_ITEM_IN_CURRENT_POSITION());
		return false;
	}
	Ref<skillengine::model::Skill> skill =
		skillengine::SkillEngine::getInstance().getSkill(player, skillid, level, player.getTarget(), parentItem->getItemTemplate());
	if (!skill)
		return false;
	const std::vector<std::unique_ptr<EffectTemplate>>& effects = effectsOf(*skill);
	if (!effects.empty()) {
		for (const std::unique_ptr<EffectTemplate>& effectTemplate : effects) {
			// Cant use transform items while already transformed
			if (player.isTransformed() && isInstance<skillengine::effect::TransformEffect>(*effectTemplate)) {
				PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_SKILL_CAN_NOT_CAST_IN_SHAPECHANGE());
				return false;
			}
			if (player.getSummon() && isInstance<skillengine::effect::SummonEffect>(*effectTemplate)) {
				PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_SKILL_SUMMON_ALREADY_HAVE_A_FOLLOWER());
				return false;
			}
		}
	}
	// also initializes effectedList for isIneffectiveHealSkill check down below
	if (!skill->canUseSkill(skillengine::properties::Properties_CastState::CAST_START))
		return false;
	if (configs::main::CustomConfig::IGNORE_POTIONS_AT_FULL_HEALTH.load() && isIneffectiveHealSkill(effects, skill->getEffectedList().snapshot())) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_NOTHING_HAPPEN());
		return false;
	}
	return true;
}

void SkillUseAction::act(gameobjects::player::Player& player, runtime::Ptr<gameobjects::Item> parentItem,
	runtime::Ptr<gameobjects::Item> /*targetItem*/, std::initializer_list<std::any> /*params*/) const {
	Ref<skillengine::model::Skill> skill =
		skillengine::SkillEngine::getInstance().getSkill(player, skillid, level, player.getTarget(), parentItem->getItemTemplate());
	if (skill) {
		skill->setItemObjectId(parentItem->getObjectId());
		skill->useSkill();
	}
}

} // namespace aion::gameserver::model::templates::item::actions
