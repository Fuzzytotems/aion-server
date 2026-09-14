#pragma once

#include "aion/gameserver/model/templates/item/actions/TuningAction.xml.h"

namespace aion::gameserver::model::templates::item::actions {

/** Java com.aionemu.gameserver.model.templates.item.actions.TuningAction. @author Rolandas */
class TuningAction : public ::aion::gameserver::model::templates::item::actions::AbstractItemAction {
#include "aion/gameserver/model/templates/item/actions/TuningAction.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::item::actions
