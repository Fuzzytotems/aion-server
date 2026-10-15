#pragma once

#include <cstdint>
#include <optional>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author Nemiroff Date: 25.01.2010
 */
class SM_ABYSS_RANK : public AionServerPacket {
private:
	runtime::Ref<model::gameobjects::player::AbyssRank> rank{};
	int32_t rankingListPosition{};

public:
	explicit SM_ABYSS_RANK(model::gameobjects::player::Player& player);
	SM_ABYSS_RANK(model::gameobjects::player::Player& player, std::optional<int32_t> rankingListPosition);
	~SM_ABYSS_RANK() override;

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
