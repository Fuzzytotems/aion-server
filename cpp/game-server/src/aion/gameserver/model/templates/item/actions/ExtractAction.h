#pragma once

#include "aion/gameserver/model/templates/item/actions/ExtractAction.xml.h"

namespace aion::gameserver::model::templates::item::actions {

/** Java com.aionemu.gameserver.model.templates.item.actions.ExtractAction. @author ATracer */
class ExtractAction : public ::aion::gameserver::model::templates::item::actions::AbstractItemAction {
#include "aion/gameserver/model/templates/item/actions/ExtractAction.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::item::actions
