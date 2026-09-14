#pragma once

#include "aion/gameserver/model/templates/tradelist/TradeListTemplate.xml.h"

namespace aion::gameserver::model::templates::tradelist {

/** Java com.aionemu.gameserver.model.templates.tradelist.TradeListTemplate. @author orz */
class TradeListTemplate : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/tradelist/TradeListTemplate.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::tradelist
