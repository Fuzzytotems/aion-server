#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/emotion/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

class SM_EMOTION_LIST : public AionServerPacket {
private:
	int8_t action{};
	std::vector<runtime::Ref<model::gameobjects::player::emotion::Emotion>> emotions{};

public:
	SM_EMOTION_LIST(int8_t action, const std::vector<runtime::Ptr<model::gameobjects::player::emotion::Emotion>>& emotions);
	~SM_EMOTION_LIST() override;

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
