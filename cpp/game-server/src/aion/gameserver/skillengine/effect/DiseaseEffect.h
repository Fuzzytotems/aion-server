#pragma once

#include "aion/gameserver/skillengine/effect/DiseaseEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.DiseaseEffect. @author kecimis */
class DiseaseEffect : public ::aion::gameserver::skillengine::effect::EffectTemplate {
#include "aion/gameserver/skillengine/effect/DiseaseEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
