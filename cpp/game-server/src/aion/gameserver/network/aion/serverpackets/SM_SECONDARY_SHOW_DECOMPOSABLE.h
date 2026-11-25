#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/model/templates/item/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * C++ interim (upstream sync foundation, docs/deviations/P4-16.md): upstream d254ea172 replaced this packet with SM_SHOW_DECOMPOSABLE /
 * SM_SELECT_DECOMPOSABLE_RESULT; until wave 5 ports them it lists the DecomposedItems of the new templates.
 *
 * @author xTz
 */
class SM_SECONDARY_SHOW_DECOMPOSABLE : public AionServerPacket {
private:
	std::vector<const model::templates::item::DecomposedItem*> itemsCollections{};
	int32_t objectId{};
public:
	SM_SECONDARY_SHOW_DECOMPOSABLE(int32_t objectId, const std::vector<const model::templates::item::DecomposedItem*>& itemsCollections);
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
