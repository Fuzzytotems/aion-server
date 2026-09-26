#pragma once

#include "aion/gameserver/skillengine/effect/AuraEffect.xml.h"

#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.AuraEffect. @author ATracer, kecimis, xTz */
class AuraEffect : public ::aion::gameserver::skillengine::effect::EffectTemplate {
#include "aion/gameserver/skillengine/effect/AuraEffect.xml.inc"
public:
	void applyEffect(model::Effect& effect) const override;

	void onPeriodicAction(model::Effect& effect) const override;

private:
	void applyAuraTo(gameserver::model::gameobjects::Creature& effected) const;

public:
	void startEffect(model::Effect& effect) const override;

	void endEffect(model::Effect& effect) const override;
};

} // namespace aion::gameserver::skillengine::effect
