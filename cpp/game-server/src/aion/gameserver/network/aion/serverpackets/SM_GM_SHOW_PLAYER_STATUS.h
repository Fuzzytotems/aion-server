#pragma once

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/stats/container/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author Yeats.
 */
class SM_GM_SHOW_PLAYER_STATUS : public AionServerPacket {
private:
	runtime::Ref<model::gameobjects::player::Player> player{};
	runtime::Ref<model::stats::container::PlayerGameStats> pgs{};
	runtime::Ref<model::stats::container::PlayerLifeStats> pls{};
	runtime::Ref<model::gameobjects::player::PlayerCommonData> pcd{};

public:
	explicit SM_GM_SHOW_PLAYER_STATUS(model::gameobjects::player::Player& player);
	~SM_GM_SHOW_PLAYER_STATUS() override;

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
