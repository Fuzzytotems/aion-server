#pragma once

#include "aion/gameserver/skillengine/effect/DPTransferEffect.xml.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.DPTransferEffect. @author Sippolo */
class DPTransferEffect : public ::aion::gameserver::skillengine::effect::EffectTemplate {
#include "aion/gameserver/skillengine/effect/DPTransferEffect.xml.inc"
public:
	void applyEffect(model::Effect& effect) const override;

	using EffectTemplate::calculate; // C++ name hiding by the declaration below (hub-headers.md §9.1)

	void calculate(model::Effect& effect) const override;

private:
	int32_t getCurrentStatValue(model::Effect& effect) const;
};

} // namespace aion::gameserver::skillengine::effect
