#pragma once

#include "aion/gameserver/model/templates/item/ItemUseLimits.xml.h"

namespace aion::gameserver::model::templates::item {

/** Java com.aionemu.gameserver.model.templates.item.ItemUseLimits. @author Rolandas */
class ItemUseLimits : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/item/ItemUseLimits.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::item
