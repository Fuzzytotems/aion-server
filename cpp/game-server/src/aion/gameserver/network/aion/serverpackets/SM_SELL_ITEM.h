#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/templates/tradelist/TradeListTemplate.h"
#include "aion/gameserver/model/templates/tradelist/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author orz, Sarynth, Artur, Neon
 */
class SM_SELL_ITEM : public AionServerPacket {
private:
	int32_t targetObjectId{};
	model::templates::tradelist::TradeNpcType tradeNpcType{};
	int32_t buyPriceRate{};
	bool showBuyTab{};
	bool showSellTab{};
	std::vector<const model::templates::tradelist::TradeListTemplate::TradeTab*> tradeTabs{};
public:
	explicit SM_SELL_ITEM(model::gameobjects::Npc& npc);
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
