#pragma once

#include "aion/gameserver/dataholders/NpcSkillData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.NpcSkillData. @author ATracer */
class NpcSkillData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/NpcSkillData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
