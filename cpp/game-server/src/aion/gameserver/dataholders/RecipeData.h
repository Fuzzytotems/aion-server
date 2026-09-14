#pragma once

#include "aion/gameserver/dataholders/RecipeData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.RecipeData. @author ATracer, MrPoke, KID */
class RecipeData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/RecipeData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
