#pragma once

#include "aion/gameserver/skillengine/effect/DummyEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.DummyEffect. @author Bobobear */
class DummyEffect : public ::aion::gameserver::skillengine::effect::EffectTemplate {
#include "aion/gameserver/skillengine/effect/DummyEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
