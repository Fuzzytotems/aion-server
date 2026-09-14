#pragma once

#include "aion/gameserver/dataholders/PetFeedData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.PetFeedData. @author Rolandas */
class PetFeedData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/PetFeedData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
