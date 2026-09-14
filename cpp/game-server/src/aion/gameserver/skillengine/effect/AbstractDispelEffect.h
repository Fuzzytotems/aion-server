#pragma once

#include "aion/gameserver/skillengine/effect/AbstractDispelEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.AbstractDispelEffect. @author kecimis */
class AbstractDispelEffect : public ::aion::gameserver::skillengine::effect::EffectTemplate {
#include "aion/gameserver/skillengine/effect/AbstractDispelEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
