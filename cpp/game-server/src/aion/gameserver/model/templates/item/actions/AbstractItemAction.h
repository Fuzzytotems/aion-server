#pragma once

#include "aion/gameserver/model/templates/item/actions/AbstractItemAction.xml.h"

namespace aion::gameserver::model::templates::item::actions {

/** Java com.aionemu.gameserver.model.templates.item.actions.AbstractItemAction. @author ATracer */
class AbstractItemAction : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/item/actions/AbstractItemAction.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::item::actions
