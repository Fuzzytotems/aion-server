#pragma once

#include "aion/gameserver/skillengine/model/SkillTemplate.xml.h"

namespace aion::gameserver::skillengine::model {

/** Java com.aionemu.gameserver.skillengine.model.SkillTemplate. @author ATracer, Wakizashi */
class SkillTemplate : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/skillengine/model/SkillTemplate.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::model
