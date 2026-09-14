#pragma once

#include "aion/gameserver/skillengine/effect/DispelNpcDebuffEffect.xml.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.DispelNpcDebuffEffect. @author kecimis */
class DispelNpcDebuffEffect : public ::aion::gameserver::skillengine::effect::AbstractDispelEffect {
#include "aion/gameserver/skillengine/effect/DispelNpcDebuffEffect.xml.inc"
public:
	void applyEffect(model::Effect& effect) const override;
};

} // namespace aion::gameserver::skillengine::effect
