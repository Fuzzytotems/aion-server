#pragma once

#include "aion/gameserver/dataholders/PetSkillData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.PetSkillData. @author ATracer */
class PetSkillData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/PetSkillData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
