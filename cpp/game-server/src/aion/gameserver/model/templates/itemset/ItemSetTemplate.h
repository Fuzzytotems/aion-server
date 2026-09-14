#pragma once

#include "aion/gameserver/model/templates/itemset/ItemSetTemplate.xml.h"

namespace aion::gameserver::model::templates::itemset {

/** Java com.aionemu.gameserver.model.templates.itemset.ItemSetTemplate. @author ATracer, Antivirus */
class ItemSetTemplate : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/itemset/ItemSetTemplate.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::itemset
