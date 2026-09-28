#pragma once

#include "aion/gameserver/skillengine/effect/ConvertHealEffect.xml.h"

#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.ConvertHealEffect. @author kecimis */
class ConvertHealEffect : public ::aion::gameserver::skillengine::effect::ShieldEffect {
#include "aion/gameserver/skillengine/effect/ConvertHealEffect.xml.inc"
public:
	void startEffect(model::Effect& effect) const override;

	void endEffect(model::Effect& effect) const override;

	model::ShieldType getType() const override;
};

} // namespace aion::gameserver::skillengine::effect
