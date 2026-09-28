#pragma once

#include <cstdint>
#include <string>

#include "aion/gameserver/model/templates/item/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author Mr. Poke, Yeats
 */
class SM_CRAFT_UPDATE : public AionServerPacket {
private:
	int32_t skillId{};
	int32_t itemId{};
	int32_t action{};
	int32_t success{};
	int32_t failure{};
	std::string itemNameL10n{};
	int32_t executionSpeed{};
	int32_t delay{};

public:
	SM_CRAFT_UPDATE(int32_t skillId, const model::templates::item::ItemTemplate* item, int32_t success, int32_t failure, int32_t action,
		int32_t executionSpeed, int32_t delay);

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
