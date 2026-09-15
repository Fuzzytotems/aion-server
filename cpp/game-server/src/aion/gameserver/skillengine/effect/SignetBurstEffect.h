#pragma once

#include "aion/gameserver/skillengine/effect/SignetBurstEffect.xml.h"

#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.SignetBurstEffect. @author ATracer, kecimis */
class SignetBurstEffect : public ::aion::gameserver::skillengine::effect::DamageEffect {
#include "aion/gameserver/skillengine/effect/SignetBurstEffect.xml.inc"
public:
	bool shouldUseBoostSpellAttackEffects() const override;

	bool shouldUseOneTimeBoostSkillAttack() const override;
};

} // namespace aion::gameserver::skillengine::effect
