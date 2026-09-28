#pragma once

#include "aion/gameserver/skillengine/effect/ShieldEffect.xml.h"

#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.ShieldEffect. @author ATracer, Wakizashi, Sippolo, kecimis */
class ShieldEffect : public ::aion::gameserver::skillengine::effect::EffectTemplate {
#include "aion/gameserver/skillengine/effect/ShieldEffect.xml.inc"
public:
	void applyEffect(model::Effect& effect) const override;

	void startEffect(model::Effect& effect) const override;

	virtual model::ShieldType getType() const;
};

} // namespace aion::gameserver::skillengine::effect
