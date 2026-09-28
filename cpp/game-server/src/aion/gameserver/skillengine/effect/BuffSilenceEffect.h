#pragma once

#include "aion/gameserver/skillengine/effect/BuffSilenceEffect.xml.h"

#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.BuffSilenceEffect. @author kecimis */
class BuffSilenceEffect : public ::aion::gameserver::skillengine::effect::SilenceEffect {
#include "aion/gameserver/skillengine/effect/BuffSilenceEffect.xml.inc"
public:
	using SilenceEffect::calculate; // C++ name hiding by the declaration below (hub-headers.md §9.1)

	void calculate(model::Effect& effect) const override;
};

} // namespace aion::gameserver::skillengine::effect
