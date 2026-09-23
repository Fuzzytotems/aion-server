#pragma once

#include "aion/gameserver/skillengine/effect/BuffBindEffect.xml.h"

#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.BuffBindEffect. @author kecimis */
class BuffBindEffect : public ::aion::gameserver::skillengine::effect::BindEffect {
#include "aion/gameserver/skillengine/effect/BuffBindEffect.xml.inc"
public:
	using BindEffect::calculate; // C++ name hiding by the declaration below (hub-headers.md §9.1)

	void calculate(model::Effect& effect) const override;
};

} // namespace aion::gameserver::skillengine::effect
