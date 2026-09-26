#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/model/templates/tradelist/TradeListTemplate.xml.h"

namespace aion::gameserver::model::templates::tradelist {

/** Java com.aionemu.gameserver.model.templates.tradelist.TradeListTemplate. @author orz */
class TradeListTemplate : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/tradelist/TradeListTemplate.xml.inc"
public:
	/** Java creates the list on first use (a write to the template); the C++ list always exists */
	const std::vector<TradeTab>& getTradeTablist() const { return tradeTablist; }

	/** Java: tradeTablist.size() (a NullPointerException before getTradeTablist() created an absent list; C++: 0) */
	int32_t getCount() const { return static_cast<int32_t>(tradeTablist.size()); }
};

} // namespace aion::gameserver::model::templates::tradelist
