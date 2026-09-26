#pragma once

#include "aion/gameserver/skillengine/effect/SkillAttackInstantEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.SkillAttackInstantEffect. @author ATracer */
class SkillAttackInstantEffect : public ::aion::gameserver::skillengine::effect::DamageEffect {
#include "aion/gameserver/skillengine/effect/SkillAttackInstantEffect.xml.inc"
public:
	bool isNoResist() const override { return cannotmiss || DamageEffect::isNoResist(); }
};

} // namespace aion::gameserver::skillengine::effect
