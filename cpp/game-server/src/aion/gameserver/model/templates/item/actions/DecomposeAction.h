#pragma once

#include "aion/gameserver/model/templates/item/actions/DecomposeAction.xml.h"

namespace aion::gameserver::model::templates::item::actions {

/** Java com.aionemu.gameserver.model.templates.item.actions.DecomposeAction. @author oslo(a00441234) */
class DecomposeAction : public ::aion::gameserver::model::templates::item::actions::AbstractItemAction {
#include "aion/gameserver/model/templates/item/actions/DecomposeAction.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::item::actions
