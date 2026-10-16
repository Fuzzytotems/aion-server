#pragma once

#include "aion/gameserver/model/templates/item/ExtractedItemsCollection.xml.h"

namespace aion::gameserver::model::templates::item {

/** Java com.aionemu.gameserver.model.templates.item.ExtractedItemsCollection. @author antness */
class ExtractedItemsCollection : public ::aion::gameserver::model::templates::item::ResultedItemsCollection {
#include "aion/gameserver/model/templates/item/ExtractedItemsCollection.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::item
