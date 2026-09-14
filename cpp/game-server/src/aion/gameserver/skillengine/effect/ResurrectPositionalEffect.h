#pragma once

#include "aion/gameserver/skillengine/effect/ResurrectPositionalEffect.xml.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.ResurrectPositionalEffect. @author Sippolo */
class ResurrectPositionalEffect : public ::aion::gameserver::skillengine::effect::ResurrectEffect {
#include "aion/gameserver/skillengine/effect/ResurrectPositionalEffect.xml.inc"
public:
	void applyEffect(model::Effect& effect) const override;
};

} // namespace aion::gameserver::skillengine::effect
