#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/limiteditems/fwd.h"
#include "aion/gameserver/model/templates/tradelist/TradeListTemplate.h"
#include "aion/gameserver/model/templates/tradelist/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author alexa026, ATracer, Sarynth, xTz, Neon
 */
class SM_TRADELIST : public AionServerPacket {
private:
	int32_t targetObjId{};
	int32_t playerObjId{};
	model::templates::tradelist::TradeNpcType tradeNpcType{};
	int32_t buyPriceModifier{};
	bool showBuyTab{};
	bool showSellTab{};
	std::vector<const model::templates::tradelist::TradeListTemplate::TradeTab*> tradeTablist{};
	std::vector<runtime::Ref<model::limiteditems::LimitedItem>> limitedItems{};
public:
	SM_TRADELIST(model::gameobjects::player::Player& player, model::gameobjects::Npc& npc,
		const model::templates::tradelist::TradeListTemplate* tlist, int32_t buyPriceModifier);
	~SM_TRADELIST() override;
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
