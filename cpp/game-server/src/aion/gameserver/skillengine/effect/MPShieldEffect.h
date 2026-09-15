#pragma once

#include "aion/gameserver/skillengine/effect/MPShieldEffect.xml.h"

#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.MPShieldEffect. @author Cheatkiller */
class MPShieldEffect : public ::aion::gameserver::skillengine::effect::ShieldEffect {
#include "aion/gameserver/skillengine/effect/MPShieldEffect.xml.inc"
public:
	model::ShieldType getType() const override;
};

} // namespace aion::gameserver::skillengine::effect
