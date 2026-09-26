#include "aion/gameserver/skillengine/effect/WeaponDualEffect.h"

#include <cstdint>
#include <string>
#include <vector>

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/skill/PlayerSkillEntry.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/skillengine/effect/EffectType.h"
#include "aion/gameserver/skillengine/effect/Effects.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"

namespace aion::gameserver::skillengine::effect {

using gameserver::model::gameobjects::player::Player;
using gameserver::model::skill::PlayerSkillEntry;
using runtime::Ptr;

namespace {

/** Java int a * b (wraps on overflow) */
constexpr int32_t mulInt(int32_t a, int32_t b) noexcept {
	return static_cast<int32_t>(static_cast<uint32_t>(a) * static_cast<uint32_t>(b));
}

/** Java int a + b (wraps on overflow) */
constexpr int32_t addInt(int32_t a, int32_t b) noexcept {
	return static_cast<int32_t>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b));
}

/** Java's implicit null check of a dereference of a static template pointer (a plain C++ dereference of nullptr is undefined) */
template <class T>
const T& nonNull(const T* value, const char* what) {
	if (value == nullptr)
		throw runtime::NullPointerException(std::string(what) + " is null");
	return *value;
}

} // namespace

void WeaponDualEffect::startEffect(model::Effect& effect) const {
	if (Ptr<Player> p = runtime::as<Player>(effect.getEffected())) {
		p->getGameStats()->setSkillEfficiency(static_cast<float>(skillEfficiency) / 100.0f);
		p->getGameStats()->setMaxDamageChance(addInt(maxDamageChance, mulInt(effect.getSkillLevel(), maxDamageDelta)));
		p->getGameStats()->setMinDamageRatio(static_cast<float>(addInt(value, mulInt(effect.getSkillLevel(), delta))) / 100.0f);
		p->getGameStats()->updateStatsVisually();
	}
}

void WeaponDualEffect::endEffect(model::Effect& effect) const {
	if (Ptr<Player> p = runtime::as<Player>(effect.getEffected())) {
		p->getGameStats()->setSkillEfficiency(0);
		p->getGameStats()->setMaxDamageChance(0);
		p->getGameStats()->setMinDamageRatio(0);
		p->getGameStats()->updateStatsVisually();
	}
	BufEffect::endEffect(effect);
}

bool WeaponDualEffect::hasDualWieldEffect(::aion::gameserver::model::gameobjects::player::Player& player) {
	if (!player.isSpawned()) { // fallback for enterWorld
		for (const Ptr<PlayerSkillEntry>& skillEntry : player.getSkillList()->getAllSkills()) {
			const Effects* effects = nonNull(skillEntry->getSkillTemplate(), "skillEntry.getSkillTemplate()").getEffects();
			if (effects != nullptr && effects->hasAnyEffectType({EffectType::WEAPONDUAL}))
				return true;
		}
	}
	return player.getGameStats()->getSkillEfficiency() != 0;
}

} // namespace aion::gameserver::skillengine::effect
