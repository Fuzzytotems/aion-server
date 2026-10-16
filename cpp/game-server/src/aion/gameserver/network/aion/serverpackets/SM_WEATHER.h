#pragma once

#include <span>
#include <vector>

#include "aion/gameserver/model/templates/world/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author ATracer, Kwazar, Nemesiss
 */
class SM_WEATHER : public AionServerPacket {
private:
	std::vector<const model::templates::world::WeatherEntry*> weatherEntries{};
public:
	explicit SM_WEATHER(std::span<const model::templates::world::WeatherEntry* const> weatherEntries);
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
