#pragma once

#include <cstdint>
#include <string>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author ATracer, Jego
 */
class SM_RESURRECT : public AionServerPacket {
private:
	std::string name{};
	int32_t skillId{};
public:
	explicit SM_RESURRECT(model::gameobjects::Creature& creature);
	SM_RESURRECT(model::gameobjects::Creature& creature, int32_t skillId);
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
