#pragma once

#include "aion/gameserver/skillengine/effect/BuffStunEffect.xml.h"

#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.BuffStunEffect. @author kecimis */
class BuffStunEffect : public ::aion::gameserver::skillengine::effect::StunEffect {
#include "aion/gameserver/skillengine/effect/BuffStunEffect.xml.inc"
public:
	using StunEffect::calculate; // C++ name hiding by the declaration below (hub-headers.md §9.1)

	void calculate(model::Effect& effect) const override;
};

} // namespace aion::gameserver::skillengine::effect
