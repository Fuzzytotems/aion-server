#pragma once

#include "aion/gameserver/model/templates/item/actions/EnchantItemAction.xml.h"

namespace aion::gameserver::model::templates::item::actions {

/** Java com.aionemu.gameserver.model.templates.item.actions.EnchantItemAction. @author Nemiroff, Wakizashi, vlog */
class EnchantItemAction : public ::aion::gameserver::model::templates::item::actions::AbstractItemAction {
#include "aion/gameserver/model/templates/item/actions/EnchantItemAction.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::item::actions
