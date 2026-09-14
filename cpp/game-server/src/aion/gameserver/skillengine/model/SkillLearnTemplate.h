#pragma once

#include "aion/gameserver/skillengine/model/SkillLearnTemplate.xml.h"

namespace aion::gameserver::skillengine::model {

/** Java com.aionemu.gameserver.skillengine.model.SkillLearnTemplate. @author ATracer, Neon */
class SkillLearnTemplate : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/skillengine/model/SkillLearnTemplate.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::model
