#pragma once

#include "aion/gameserver/skillengine/effect/WeaponMasteryEffect.xml.h"

#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.WeaponMasteryEffect. @author ATracer */
class WeaponMasteryEffect : public ::aion::gameserver::skillengine::effect::BufEffect {
#include "aion/gameserver/skillengine/effect/WeaponMasteryEffect.xml.inc"
public:
	void startEffect(model::Effect& effect) const override;
};

} // namespace aion::gameserver::skillengine::effect
