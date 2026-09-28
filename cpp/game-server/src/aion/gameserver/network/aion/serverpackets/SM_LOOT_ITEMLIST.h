#pragma once

#include <cstdint>
#include <unordered_set>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/drop/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author alexa026, Avol, Metos, ATracer, KID, Sykra
 */
class SM_LOOT_ITEMLIST : public AionServerPacket {
private:
	int32_t targetObjectId{};
	bool teamMembersNearby{};
	std::vector<runtime::Ref<model::drop::DropItem>> dropItems{};
public:
	SM_LOOT_ITEMLIST(model::gameobjects::DropNpc& dropNpc, const std::unordered_set<runtime::Ptr<model::drop::DropItem>>& setItems,
		model::gameobjects::player::Player& player);
	~SM_LOOT_ITEMLIST() override;

	/** C++ only: writeImpl reads the connection, so every recipient gets its own serialization (runtime-architecture.md §8.3) */
	Recipients recipients() const noexcept override { return Recipients::PER_RECIPIENT; }
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
