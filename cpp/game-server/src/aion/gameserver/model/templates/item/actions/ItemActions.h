#pragma once

#include "aion/gameserver/model/templates/item/actions/ItemActions.xml.h"

namespace aion::gameserver::model::templates::item::actions {

/** Java com.aionemu.gameserver.model.templates.item.actions.ItemActions. @author ATracer */
class ItemActions : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/item/actions/ItemActions.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::item::actions
