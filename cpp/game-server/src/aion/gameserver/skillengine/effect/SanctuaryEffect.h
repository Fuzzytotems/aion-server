#pragma once

#include "aion/gameserver/skillengine/effect/SanctuaryEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.SanctuaryEffect. @author kecimis, Cheatkiller, add AbnormalState */
class SanctuaryEffect : public ::aion::gameserver::skillengine::effect::EffectTemplate {
#include "aion/gameserver/skillengine/effect/SanctuaryEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
