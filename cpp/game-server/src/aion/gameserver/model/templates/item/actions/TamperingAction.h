#pragma once

#include "aion/gameserver/model/templates/item/actions/TamperingAction.xml.h"

namespace aion::gameserver::model::templates::item::actions {

/** Java com.aionemu.gameserver.model.templates.item.actions.TamperingAction. @author Rolandas */
class TamperingAction : public ::aion::gameserver::model::templates::item::actions::AbstractItemAction {
#include "aion/gameserver/model/templates/item/actions/TamperingAction.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::item::actions
