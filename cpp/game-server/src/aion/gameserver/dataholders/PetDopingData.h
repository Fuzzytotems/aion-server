#pragma once

#include "aion/gameserver/dataholders/PetDopingData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.PetDopingData. @author Rolandas */
class PetDopingData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/PetDopingData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
