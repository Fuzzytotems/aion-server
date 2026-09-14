#pragma once

#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author Rolandas
 */
class SM_HOUSE_OBJECTS : public AionServerPacket {
private:
	std::vector<runtime::Ref<model::gameobjects::HouseObject>> objects{};

public:
	explicit SM_HOUSE_OBJECTS(const std::vector<runtime::Ptr<model::gameobjects::HouseObject>>& objects);
	~SM_HOUSE_OBJECTS() override;

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
