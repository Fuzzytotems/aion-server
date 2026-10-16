#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/templates/housing/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author Rolandas
 */
class SM_OBJECT_USE_UPDATE : public AionServerPacket {
private:
	int32_t usingPlayerId{};
	int32_t ownerPlayerId{};
	int32_t useCount{};
	const model::templates::housing::UseItemAction* action{}; // Java: = null
	runtime::Ref<model::gameobjects::HouseObject> object{}; // Java HouseObject<?> (erased, hub-headers.md §8.1)

public:
	SM_OBJECT_USE_UPDATE(int32_t usingPlayerId, int32_t ownerPlayerId, int32_t useCount, model::gameobjects::HouseObject& object);
	~SM_OBJECT_USE_UPDATE() override;

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
