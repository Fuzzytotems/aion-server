#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/model/templates/item/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author xTz
 */
class SM_SECONDARY_SHOW_DECOMPOSABLE : public AionServerPacket {
private:
	std::vector<const model::templates::item::ResultedItem*> itemsCollections{};
	int32_t objectId{};
public:
	SM_SECONDARY_SHOW_DECOMPOSABLE(int32_t objectId, const std::vector<const model::templates::item::ResultedItem*>& itemsCollections);
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
