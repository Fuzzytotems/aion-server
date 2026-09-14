#pragma once

#include "aion/gameserver/model/templates/item/actions/RemodelAction.xml.h"

namespace aion::gameserver::model::templates::item::actions {

/** Java com.aionemu.gameserver.model.templates.item.actions.RemodelAction. @author Rolandas */
class RemodelAction : public ::aion::gameserver::model::templates::item::actions::AbstractItemAction {
#include "aion/gameserver/model/templates/item/actions/RemodelAction.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::item::actions
