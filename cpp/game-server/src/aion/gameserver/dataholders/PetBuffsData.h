#pragma once

#include "aion/gameserver/dataholders/PetBuffsData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.PetBuffsData. @author Rolandas */
class PetBuffsData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/PetBuffsData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
