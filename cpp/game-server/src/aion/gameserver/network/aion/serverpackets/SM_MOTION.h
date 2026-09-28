#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/motion/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author MrPoke
 */
class SM_MOTION : public AionServerPacket {
public:
	int8_t action{};
	int16_t motionId{};
	int32_t remainingTime{};
	int32_t playerId{};
	std::unordered_map<int32_t, runtime::Ref<model::gameobjects::player::motion::Motion>> activeMotions{};
	std::vector<runtime::Ref<model::gameobjects::player::motion::Motion>> motions{};
	int8_t type{};
	explicit SM_MOTION(const std::vector<runtime::Ptr<model::gameobjects::player::motion::Motion>>& motions);
	SM_MOTION(int16_t motionId, int32_t remainingTime);
	SM_MOTION(int16_t motionId, int8_t type);
	explicit SM_MOTION(int16_t motionId);
	SM_MOTION(int32_t playerId, const std::unordered_map<int32_t, runtime::Ptr<model::gameobjects::player::motion::Motion>>& activeMotions);
	~SM_MOTION() override;
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
