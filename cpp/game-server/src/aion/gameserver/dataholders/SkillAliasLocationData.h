#pragma once

#include "aion/gameserver/dataholders/SkillAliasLocationData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.SkillAliasLocationData. */
class SkillAliasLocationData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/SkillAliasLocationData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
