#pragma once

#include "aion/gameserver/dataholders/PetData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.PetData. @author IlBuono */
class PetData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/PetData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
