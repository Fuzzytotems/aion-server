#pragma once

#include "aion/gameserver/skillengine/effect/DispelNpcBuffEffect.xml.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.DispelNpcBuffEffect. @author kecimis */
class DispelNpcBuffEffect : public ::aion::gameserver::skillengine::effect::AbstractDispelEffect {
#include "aion/gameserver/skillengine/effect/DispelNpcBuffEffect.xml.inc"
public:
	using AbstractDispelEffect::applyEffect; // C++ name hiding by the declaration below (hub-headers.md §9.1)

	void applyEffect(model::Effect& effect) const override;
};

} // namespace aion::gameserver::skillengine::effect
