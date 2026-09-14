#pragma once

#include "aion/gameserver/model/templates/item/actions/ApExtractAction.xml.h"

namespace aion::gameserver::model::templates::item::actions {

/** Java com.aionemu.gameserver.model.templates.item.actions.ApExtractAction. @author Rolandas, Luzien */
class ApExtractAction : public ::aion::gameserver::model::templates::item::actions::AbstractItemAction {
#include "aion/gameserver/model/templates/item/actions/ApExtractAction.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::item::actions
