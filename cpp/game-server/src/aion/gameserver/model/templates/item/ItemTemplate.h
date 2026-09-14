#pragma once

#include "aion/gameserver/model/templates/item/ItemTemplate.xml.h"

namespace aion::gameserver::model::templates::item {

/** Java com.aionemu.gameserver.model.templates.item.ItemTemplate. @author Luno, ATracer */
class ItemTemplate : public ::aion::gameserver::model::templates::VisibleObjectTemplate {
#include "aion/gameserver/model/templates/item/ItemTemplate.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::item
