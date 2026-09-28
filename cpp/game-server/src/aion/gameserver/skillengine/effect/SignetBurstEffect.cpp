#include "aion/gameserver/skillengine/effect/SignetBurstEffect.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/controllers/attack/AttackUtil.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/SignetDataTemplates.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/templates/detail/JavaCasts.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/SignetData.h"
#include "aion/gameserver/skillengine/model/SignetEnum.h"

namespace aion::gameserver::skillengine::effect {

using runtime::Ptr;

namespace {

/** Java int a * b (wraps on overflow) */
constexpr int32_t mulInt(int32_t a, int32_t b) noexcept {
	return static_cast<int32_t>(static_cast<uint32_t>(a) * static_cast<uint32_t>(b));
}

/**
 * Java SignetEnum.valueOf(signet): IllegalArgumentException for a name that is no constant (a template without signet= is a NullPointerException
 * in Java; the C++ member cannot tell a missing attribute from an empty one, and the data has neither: 68 of 68 <signetburst> carry a constant)
 */
model::SignetEnum signetEnumOf(const std::string& signet) {
	if (std::optional<model::SignetEnum> value = xml::enumFromName<model::SignetEnum>(signet))
		return *value;
	throw runtime::IllegalArgumentException("No enum constant com.aionemu.gameserver.skillengine.model.SignetEnum." + signet);
}

} // namespace

void SignetBurstEffect::calculateDamage(model::Effect& effect) const {
	const Ptr<model::Effect> signetEffect = effect.getEffected()->getEffectController()->getAbnormalEffect(signet);
	int32_t valueWithDelta = calculateBaseValue(effect);
	int32_t effectProb = 0;
	int32_t signetLvl = std::min(signetlvl, !signetEffect ? 0 : signetEffect->getSkillLevel());
	const model::SignetData* signetData = dataholders::DataManager::SIGNET_DATA_TEMPLATES->getSignetData(signetEnumOf(signet), signetLvl);
	if (signetData != nullptr) {
		// Java `valueWithDelta *= signetData.getDamageMultiplier()`: a float product narrowed back to int (the saturating cast)
		valueWithDelta = gameserver::model::templates::detail::floatToInt(static_cast<float>(valueWithDelta) * signetData->getDamageMultiplier());
		effectProb = mulInt(signetData->getAddEffectProb(), addEffectProbMultiplier);
	}
	effect.setSignetBurstedCount(signetLvl);
	controllers::attack::AttackUtil::calculateSkillResult(effect, valueWithDelta, this, false);
	effect.setLaunchSubEffect(commons::utils::Rnd::chance() < static_cast<float>(effectProb));
	if (signetEffect)
		signetEffect->endEffect();
}

void SignetBurstEffect::calculate(model::Effect& effect) const {
	const Ptr<model::Effect> signetEffect = effect.getEffected()->getEffectController()->getAbnormalEffect(signet);
	if (!EffectTemplate::calculate(effect, std::nullopt, std::nullopt)) {
		if (signetEffect) {
			signetEffect->endEffect();
		}
	}
}

bool SignetBurstEffect::shouldUseBoostSpellAttackEffects() const {
	return false;
}

bool SignetBurstEffect::shouldUseOneTimeBoostSkillAttack() const {
	return false;
}

} // namespace aion::gameserver::skillengine::effect
