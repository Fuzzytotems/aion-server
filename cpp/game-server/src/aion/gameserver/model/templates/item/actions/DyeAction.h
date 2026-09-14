#pragma once

#include "aion/gameserver/model/templates/item/actions/DyeAction.xml.h"

namespace aion::gameserver::model::templates::item::actions {

/** Java com.aionemu.gameserver.model.templates.item.actions.DyeAction. @author IceReaper, Neon */
class DyeAction : public ::aion::gameserver::model::templates::item::actions::AbstractItemAction {
#include "aion/gameserver/model/templates/item/actions/DyeAction.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::item::actions
