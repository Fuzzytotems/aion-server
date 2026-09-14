#pragma once

#include "aion/gameserver/model/templates/itemset/PartBonus.xml.h"

namespace aion::gameserver::model::templates::itemset {

/** Java com.aionemu.gameserver.model.templates.itemset.PartBonus. @author ATracer */
class PartBonus : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/itemset/PartBonus.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::itemset
