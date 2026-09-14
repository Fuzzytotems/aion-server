#pragma once

#include "aion/gameserver/model/templates/item/actions/ReadAction.xml.h"

namespace aion::gameserver::model::templates::item::actions {

/** Java com.aionemu.gameserver.model.templates.item.actions.ReadAction. */
class ReadAction : public ::aion::gameserver::model::templates::item::actions::AbstractItemAction {
#include "aion/gameserver/model/templates/item/actions/ReadAction.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::item::actions
