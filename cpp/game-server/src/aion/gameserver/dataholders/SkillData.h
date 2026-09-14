#pragma once

#include "aion/gameserver/dataholders/SkillData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.SkillData. @author ATracer, Neon */
class SkillData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/SkillData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
