#pragma once

#include <cstdint>
#include <unordered_map>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/items/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author ATracer
 */
class SM_ITEM_COOLDOWN : public AionServerPacket {
private:
	std::unordered_map<int32_t, runtime::Ref<model::items::ItemCooldown>> cooldowns{};

public:
	explicit SM_ITEM_COOLDOWN(const std::unordered_map<int32_t, runtime::Ptr<model::items::ItemCooldown>>& cooldowns);
	~SM_ITEM_COOLDOWN() override;

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
