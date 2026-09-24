#include "aion/gameserver/skillengine/effect/HealOverTimeEffect.h"

#include <algorithm>
#include <cstdint>
#include <optional>

#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/model/stats/container/PlayerLifeStats.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS_LOG.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS_TYPE.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/EffectReserved.h"
#include "aion/gameserver/skillengine/model/HealType.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"

namespace aion::gameserver::skillengine::effect {

using gameserver::model::gameobjects::player::Player;
using network::aion::serverpackets::SM_ATTACK_STATUS_LOG;
using network::aion::serverpackets::SM_ATTACK_STATUS_TYPE;

namespace {

/**
 * Java EffectReserved.ResourceType.of(HealType): `valueOf(healType.name())` (EffectReserved.java:34-36). No companion header of the generated
 * enum declares it (header request, docs/deviations/P5-03.md; AbstractHealEffect.cpp has the same helper); every HealType name is a ResourceType
 * constant, so valueOf never throws.
 */
model::EffectReserved::ResourceType resourceTypeOf(model::HealType healType) {
	return *xml::enumFromName<model::EffectReserved::ResourceType>(xml::enumName(healType));
}

/** Java int a - b (wraps on overflow) */
constexpr int32_t subInt(int32_t a, int32_t b) noexcept {
	return static_cast<int32_t>(static_cast<uint32_t>(a) - static_cast<uint32_t>(b));
}

} // namespace

void HealOverTimeEffect::calculate(model::Effect& effect) const {
	// Java: super.calculate(effect, null, null) - AbstractOverTimeEffect does not override it, so it is EffectTemplate's
	if (!AbstractOverTimeEffect::calculate(effect, std::nullopt, std::nullopt))
		return;

	effect.addSuccessEffect(this);
}

void HealOverTimeEffect::startEffect(model::Effect& effect, model::HealType healType) const {
	effect.setReserveds(
		*model::EffectReserved::create(position, calculateSnapshotHealValue(effect, healType), resourceTypeOf(healType), false, false), true);
	AbstractOverTimeEffect::startEffect(effect, std::nullopt);
}

void HealOverTimeEffect::onPeriodicAction(model::Effect& effect, model::HealType healType) const {
	runtime::Ptr<gameserver::model::gameobjects::Creature> effected = effect.getEffected();

	int32_t currentValue = getCurrentStatValue(effect);
	int32_t maxCurValue = getMaxStatValue(effect);
	int32_t possibleHealValue = effect.getReserveds(position)->getValue();

	if (healType == model::HealType::HP)
		possibleHealValue = applyHealDeboost(effect, possibleHealValue);

	int32_t healValue = std::min(subInt(maxCurValue, currentValue), possibleHealValue);

	switch (healType) {
		case model::HealType::HP:
			effected->getLifeStats()->increaseHp(SM_ATTACK_STATUS_TYPE::HP, healValue, effect, SM_ATTACK_STATUS_LOG::HEAL);
			break;
		case model::HealType::MP:
			effected->getLifeStats()->increaseMp(SM_ATTACK_STATUS_TYPE::MP, healValue, effect.getSkillId(), SM_ATTACK_STATUS_LOG::MPHEAL);
			break;
		case model::HealType::FP:
			runtime::cast<Player>(effected)->getLifeStats()->increaseFp(SM_ATTACK_STATUS_TYPE::FP, healValue, effect.getSkillId(),
				SM_ATTACK_STATUS_LOG::FPHEAL);
			break;
		case model::HealType::DP:
			runtime::cast<Player>(effected)->getCommonData()->addDp(healValue);
			break;
	}
}

bool HealOverTimeEffect::isPercent() const {
	return percent;
}

bool HealOverTimeEffect::allowHpHealBoost(model::Effect& effect) const {
	return effect.getSkillTemplate()->isApplyHealBoostBonus();
}

bool HealOverTimeEffect::allowHpHealSkillDeboost(model::Effect& effect) const {
	return effect.getSkillTemplate()->isApplyHealBoostBonus();
}

int32_t HealOverTimeEffect::calculateBaseHealValue(model::Effect& effect) const {
	return calculateBaseValue(effect);
}

} // namespace aion::gameserver::skillengine::effect
