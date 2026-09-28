#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/templates/tradelist/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author MrPoke
 */
class SM_TRADE_IN_LIST : public AionServerPacket {
private:
	runtime::Ref<model::gameobjects::Npc> npc{};
	const model::templates::tradelist::TradeListTemplate* tlist{};
	int32_t buyPriceModifier{};
public:
	SM_TRADE_IN_LIST(model::gameobjects::Npc& npc, const model::templates::tradelist::TradeListTemplate* tlist, int32_t buyPriceModifier);
	~SM_TRADE_IN_LIST() override;
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
