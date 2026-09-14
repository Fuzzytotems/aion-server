#pragma once

#include "aion/gameserver/model/templates/item/ReturnLocList.xml.h"

namespace aion::gameserver::model::templates::item {

/** Java com.aionemu.gameserver.model.templates.item.ReturnLocList. @author ginho1 */
class ReturnLocList : public ::aion::gameserver::model::templates::item::ResultedItemsCollection {
#include "aion/gameserver/model/templates/item/ReturnLocList.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::item
