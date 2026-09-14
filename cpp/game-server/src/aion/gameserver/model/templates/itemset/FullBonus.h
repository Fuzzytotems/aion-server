#pragma once

#include "aion/gameserver/model/templates/itemset/FullBonus.xml.h"

namespace aion::gameserver::model::templates::itemset {

/** Java com.aionemu.gameserver.model.templates.itemset.FullBonus. @author ATracer */
class FullBonus : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/itemset/FullBonus.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::itemset
