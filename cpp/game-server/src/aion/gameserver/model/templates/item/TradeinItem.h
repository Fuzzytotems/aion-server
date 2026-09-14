#pragma once

#include "aion/gameserver/model/templates/item/TradeinItem.xml.h"

namespace aion::gameserver::model::templates::item {

/** Java com.aionemu.gameserver.model.templates.item.TradeinItem. @author MrPoke */
class TradeinItem : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/item/TradeinItem.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::item
