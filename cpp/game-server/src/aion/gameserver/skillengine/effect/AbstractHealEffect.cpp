#include "aion/gameserver/skillengine/effect/AbstractHealEffect.h"

#include <algorithm>
#include <cstdint>
#include <optional>

#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/model/stats/container/PlayerLifeStats.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS_LOG.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS_TYPE.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/effect/AbnormalState.h"
#include "aion/gameserver/skillengine/effect/ProcHealInstantEffect.h"
#include "aion/gameserver/skillengine/effect/ProcMPHealInstantEffect.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/EffectReserved.h"
#include "aion/gameserver/skillengine/model/HealType.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"

namespace aion::gameserver::skillengine::effect {

using gameserver::model::gameobjects::Creature;
using gameserver::model::gameobjects::player::Player;
using network::aion::serverpackets::SM_ATTACK_STATUS_LOG;
using network::aion::serverpackets::SM_ATTACK_STATUS_TYPE;

namespace {

/**
 * Java EffectReserved.ResourceType.of(HealType): `valueOf(healType.name())` (EffectReserved.java:34-36). No companion header of the generated
 * enum declares it (header request, docs/deviations/P5-03.md); every HealType name is a ResourceType constant, so valueOf never throws.
 */
model::EffectReserved::ResourceType resourceTypeOf(model::HealType healType) {
	return *xml::enumFromName<model::EffectReserved::ResourceType>(xml::enumName(healType));
}

/** Java int a - b (wraps on overflow) */
constexpr int32_t subInt(int32_t a, int32_t b) noexcept {
	return static_cast<int32_t>(static_cast<uint32_t>(a) - static_cast<uint32_t>(b));
}

} // namespace

void AbstractHealEffect::calculate(model::Effect& effect, model::HealType healType) const {
	if (!EffectTemplate::calculate(effect, std::nullopt, std::nullopt))
		return;
	effect.setReserveds(*model::EffectReserved::create(position, calculateHealValue(effect, healType), resourceTypeOf(healType), false), false);
}

void AbstractHealEffect::applyEffect(model::Effect& effect, model::HealType healType) const {
	runtime::Ptr<Creature> effected = effect.getEffected();
	int32_t healValue = effect.getReserveds(position)->getValue();
	switch (healType) {
		case model::HealType::HP:
			if (dynamic_cast<const ProcHealInstantEffect*>(this) != nullptr) // item heal, eg potions
				effected->getLifeStats()->increaseHp(SM_ATTACK_STATUS_TYPE::HP, healValue, *effect.getEffector());
			else
				effected->getLifeStats()->increaseHp(SM_ATTACK_STATUS_TYPE::REGULAR, healValue, *effect.getEffector());
			break;
		case model::HealType::MP:
			if (dynamic_cast<const ProcMPHealInstantEffect*>(this) != nullptr) // item heal, eg potions
				effected->getLifeStats()->increaseMp(SM_ATTACK_STATUS_TYPE::MP, healValue, 0, SM_ATTACK_STATUS_LOG::REGULAR);
			else
				effected->getLifeStats()->increaseMp(SM_ATTACK_STATUS_TYPE::HEAL_MP, healValue, 0, SM_ATTACK_STATUS_LOG::REGULAR);
			break;
		case model::HealType::FP:
			if (!runtime::as<Player>(effected))
				return;
			runtime::cast<Player>(effected)->getLifeStats()->increaseFp(SM_ATTACK_STATUS_TYPE::FP_RINGS, healValue, 0, SM_ATTACK_STATUS_LOG::REGULAR);
			break;
		case model::HealType::DP:
			runtime::cast<Player>(effected)->getCommonData()->addDp(healValue);
			break;
	}
}

bool AbstractHealEffect::isPercent() const {
	return percent;
}

bool AbstractHealEffect::allowHpHealBoost(model::Effect& effect) const {
	return effect.getSkillTemplate()->isApplyHealBoostBonus();
}

bool AbstractHealEffect::allowHpHealSkillDeboost(model::Effect& /*effect*/) const {
	return true;
}

int32_t AbstractHealEffect::calculateBaseHealValue(model::Effect& effect) const {
	return calculateBaseValue(effect);
}

int32_t AbstractHealEffect::calculateHealValue(model::Effect& effect, model::HealType type) const {
	if (type == model::HealType::HP && effect.getEffected()->getEffectController()->isAbnormalSet(AbnormalState::DISEASE))
		return 0;
	int32_t cap = subInt(getMaxStatValue(effect), getCurrentStatValue(effect));
	int32_t healValue = HealEffectTemplate::calculateHealValue(effect, type);
	return std::min(cap, healValue);
}

} // namespace aion::gameserver::skillengine::effect
