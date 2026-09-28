#pragma once

#include "aion/gameserver/skillengine/effect/BuffSleepEffect.xml.h"

#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.BuffSleepEffect. @author kecimis */
class BuffSleepEffect : public ::aion::gameserver::skillengine::effect::SleepEffect {
#include "aion/gameserver/skillengine/effect/BuffSleepEffect.xml.inc"
public:
	using SleepEffect::calculate; // C++ name hiding by the declaration below (hub-headers.md §9.1)

	void calculate(model::Effect& effect) const override;

	void startEffect(model::Effect& effect) const override;
};

} // namespace aion::gameserver::skillengine::effect
