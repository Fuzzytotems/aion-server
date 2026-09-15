#pragma once

#include "aion/gameserver/skillengine/effect/ProtectEffect.xml.h"

#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.ProtectEffect. @author Sippolo, kecimis */
class ProtectEffect : public ::aion::gameserver::skillengine::effect::ShieldEffect {
#include "aion/gameserver/skillengine/effect/ProtectEffect.xml.inc"
public:
	model::ShieldType getType() const override;
};

} // namespace aion::gameserver::skillengine::effect
