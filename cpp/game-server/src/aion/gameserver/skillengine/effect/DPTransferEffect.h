#pragma once

#include "aion/gameserver/skillengine/effect/DPTransferEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.DPTransferEffect. @author Sippolo */
class DPTransferEffect : public ::aion::gameserver::skillengine::effect::EffectTemplate {
#include "aion/gameserver/skillengine/effect/DPTransferEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
