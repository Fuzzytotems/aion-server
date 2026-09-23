#pragma once

#include "aion/gameserver/skillengine/effect/AbstractDispelEffect.xml.h"

#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.AbstractDispelEffect. @author kecimis */
class AbstractDispelEffect : public ::aion::gameserver::skillengine::effect::EffectTemplate {
#include "aion/gameserver/skillengine/effect/AbstractDispelEffect.xml.inc"
public:
	using EffectTemplate::applyEffect; // C++ name hiding by the declaration below (hub-headers.md §9.1)

	void applyEffect(model::Effect& effect, model::DispelCategoryType type, model::SkillTargetSlot slot) const;
};

} // namespace aion::gameserver::skillengine::effect
