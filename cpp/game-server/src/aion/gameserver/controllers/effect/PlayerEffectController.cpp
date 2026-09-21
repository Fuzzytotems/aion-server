#include "aion/gameserver/controllers/effect/PlayerEffectController.h"

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/controllers/effect/CumulativeResist.h"
#include "aion/gameserver/controllers/effect/CumulativeResistType.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/alliance/PlayerAllianceService.h"
#include "aion/gameserver/model/team/common/legacy/GroupEvent.h"
#include "aion/gameserver/model/team/common/legacy/PlayerAllianceEvent.h"
#include "aion/gameserver/model/team/group/PlayerGroupService.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ABNORMAL_STATE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/event/EventService.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/SkillTargetSlotInfo.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::controllers::effect {

using skillengine::model::Effect;
using skillengine::model::SkillTargetSlot;

namespace {

/** Java: effect.getTargetSlot().getId() (a NullPointerException for a null slot) */
int32_t targetSlotIdOf(Effect& effect) {
	std::optional<SkillTargetSlot> slot = effect.getTargetSlot();
	if (!slot)
		throw runtime::NullPointerException("effect target slot is null");
	return getId(*slot);
}

} // namespace

PlayerEffectController::PlayerEffectController(model::gameobjects::Creature& ownerValue) : EffectController(ownerValue) {
}

PlayerEffectController::~PlayerEffectController() = default;

void PlayerEffectController::addEffect(skillengine::model::Effect& effect) {
	AION_UNPORTED();
}

void PlayerEffectController::clearEffect(skillengine::model::Effect& effect, bool broadcast) {
	AION_UNPORTED();
}

model::gameobjects::player::Player& PlayerEffectController::getOwner() const {
	return static_cast<model::gameobjects::player::Player&>(EffectController::getOwner());
}

void PlayerEffectController::removeAllEffects(bool logout) {
	EffectController::removeAllEffects(logout);
	if (!logout)
		updatePlayerIconsAndGroup(nullptr);
}

void PlayerEffectController::removeNonStorableEffectsForLogout() {
	for (const runtime::Ptr<Effect>& e : getAllEffects()) {
		if (!e->canSaveOnLogout())
			e->endEffect(false);
	}
}

void PlayerEffectController::updatePlayerIconsAndGroup(runtime::Ptr<skillengine::model::Effect> effect) {
	if (!effect || !effect->isPassive()) {
		updatePlayerEffectIcons(effect);
		int32_t slot = !effect ? skillengine::model::SKILL_TARGET_SLOT_FULLSLOTS : targetSlotIdOf(*effect);
		if (getOwner().isInGroup()) {
			model::team::group::PlayerGroupService::updateGroup(getOwner(), model::team::common::legacy::GroupEvent::MOVEMENT);
			model::team::group::PlayerGroupService::updateGroupEffects(getOwner(), slot);
		} else if (getOwner().isInAlliance()) {
			model::team::alliance::PlayerAllianceService::updateAlliance(getOwner(), model::team::common::legacy::PlayerAllianceEvent::MOVEMENT);
			model::team::alliance::PlayerAllianceService::updateAllianceEffects(getOwner(), slot);
		}
	}
}

void PlayerEffectController::updatePlayerEffectIcons(runtime::Ptr<skillengine::model::Effect> effect) {
	int32_t slot = effect ? targetSlotIdOf(*effect) : skillengine::model::SKILL_TARGET_SLOT_FULLSLOTS;
	std::vector<runtime::Ptr<Effect>> effects = getAbnormalEffectsToShow();
	utils::PacketSendUtility::sendPacket(getOwner(), network::aion::serverpackets::SM_ABNORMAL_STATE(effects, getAbnormals(), slot));
}

bool PlayerEffectController::checkDuelCondition(skillengine::model::Effect& effect) {
	AION_UNPORTED();
}

void PlayerEffectController::addSavedEffect(int32_t skillId, int32_t skillLvl, int32_t remainingTime, int64_t endTime,
	const skillengine::model::Effect_ForceType* forceType, const std::unordered_set<int32_t>* magicalCriticalPositions) {
	if (services::event::EventService::getInstance().isInactiveEventForceType(forceType))
		return;
	const skillengine::model::SkillTemplate* skillTemplate = dataholders::DataManager::SKILL_DATA->getSkillTemplate(skillId);

	if (remainingTime <= 0)
		return;
	if (configs::main::CustomConfig::ABYSSXFORM_LOGOUT.load()) {
		if (skillTemplate == nullptr) // Java: template.isDeityAvatar() on a null template
			throw runtime::NullPointerException("skill template " + std::to_string(skillId) + " is null");
		if (skillTemplate->isDeityAvatar()) {
			if (commons::utils::currentTimeMillis() >= endTime)
				return;
			else
				remainingTime = static_cast<int32_t>(endTime - commons::utils::currentTimeMillis());
		}
	}

	// M5a warn stub for the rest of the Java method (plan D3): restoring the effect needs EffectController.put, Effect.addAllEffectToSucess and
	// Effect.startEffect, all outside the M5a subset (P5-02/P5-03). The checks above are Java's, so a row Java drops is dropped here too; a row
	// Java would restore is skipped with one warning instead of aborting enter-world (docs/deviations/P5-02.md). Nothing writes player_effects at
	// M5a (the passive effects of O-09 are a warn stub as well), so the table is empty for the D1 profile.
	// Java (PlayerEffectController.java:110-117): new Effect(owner, owner, template, skillLvl, remainingTime, forceType, false,
	// magicalCriticalPositions); put(effect); effect.addAllEffectToSucess(); effect.startEffect(); and, unless the target slot is NOSHOW,
	// PacketSendUtility.sendPacket(owner, new SM_ABNORMAL_STATE(List.of(effect), getAbnormals(), SkillTargetSlot.FULLSLOTS)).
	AION_PARTIAL("a saved effect is not restored: EffectController.put and Effect.startEffect are not ported yet (M5a)");
}

void PlayerEffectController::removeAllEffects() {
	EffectController::removeAllEffects();
	SYNCHRONIZED(cumulativeResistInfo) {
		cumulativeResistInfo.clear();
	}
}

bool PlayerEffectController::canRemoveOnDie(skillengine::model::Effect& effect) {
	if (!EffectController::canRemoveOnDie(effect))
		return false;
	if (keepBuffsOnDie.get())
		return effect.getTargetSlot() == SkillTargetSlot::DEBUFF;
	return true;
}

int64_t PlayerEffectController::calculateAndApplyCumulativeResistDuration(CumulativeResistType type, int64_t duration) {
	AION_UNPORTED();
}

int32_t PlayerEffectController::getCumulativeResistance(CumulativeResistType type) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::controllers::effect
