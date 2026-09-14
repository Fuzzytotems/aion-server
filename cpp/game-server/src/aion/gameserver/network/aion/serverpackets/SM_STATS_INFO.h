#pragma once

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/stats/container/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * In this packet Server is sending User Info?
 *
 * @author -Nemesiss-, Luno, ginho1
 */
class SM_STATS_INFO : public AionServerPacket {
private:
	runtime::Ref<model::gameobjects::player::Player> player{};
	runtime::Ref<model::stats::container::PlayerGameStats> pgs{};
	runtime::Ref<model::stats::container::PlayerLifeStats> pls{};
	runtime::Ref<model::gameobjects::player::PlayerCommonData> pcd{};
public:
	explicit SM_STATS_INFO(model::gameobjects::player::Player& player);
	~SM_STATS_INFO() override;
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
