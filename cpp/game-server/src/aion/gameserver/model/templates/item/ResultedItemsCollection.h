#pragma once

#include "aion/gameserver/model/templates/item/ResultedItemsCollection.xml.h"

namespace aion::gameserver::model::templates::item {

/** Java com.aionemu.gameserver.model.templates.item.ResultedItemsCollection. @author antness */
class ResultedItemsCollection : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/item/ResultedItemsCollection.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::item
