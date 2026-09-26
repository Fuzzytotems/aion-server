#pragma once

#include <string>

#include "aion/gameserver/model/templates/item/TradeinItem.xml.h"

namespace aion::gameserver::model::templates::item {

/** Java com.aionemu.gameserver.model.templates.item.TradeinItem. @author MrPoke */
class TradeinItem : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/item/TradeinItem.xml.inc"
public:
	std::string toString() const { return "TradeinItem [id=" + std::to_string(id) + ", price=" + std::to_string(price) + "]"; }
};

} // namespace aion::gameserver::model::templates::item
