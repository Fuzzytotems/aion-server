#pragma once

#include "aion/gameserver/skillengine/effect/DummyEffect.xml.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.DummyEffect. @author Bobobear */
class DummyEffect : public ::aion::gameserver::skillengine::effect::EffectTemplate {
#include "aion/gameserver/skillengine/effect/DummyEffect.xml.inc"
public:
	void applyEffect(model::Effect& effect) const override;
};

} // namespace aion::gameserver::skillengine::effect
