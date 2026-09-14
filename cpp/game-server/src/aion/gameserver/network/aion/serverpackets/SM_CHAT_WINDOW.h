#pragma once

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author ginho1, Cheatkiller, Neon
 */
class SM_CHAT_WINDOW : public AionServerPacket {
private:
	runtime::Ref<model::gameobjects::player::Player> target{};
	bool isGroup{};

public:
	SM_CHAT_WINDOW(model::gameobjects::player::Player& target, bool isGroup);
	~SM_CHAT_WINDOW() override;

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
