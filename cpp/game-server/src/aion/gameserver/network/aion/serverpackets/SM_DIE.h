#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"
#include "aion/gameserver/world/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author orz, Sarynth, Rhys2002
 */
class SM_DIE : public AionServerPacket {
private:
	bool allowReviveBySkill{};
	bool allowReviveByItem{};
	int32_t remainingKiskTimeSeconds{};
	bool allowInstanceRevive{};
	bool invasion{};

public:
	explicit SM_DIE(model::gameobjects::player::Player& player);

protected:
	void writeImpl(AionConnection* con) override;

private:
	world::WorldMapType getInvasionWorld(model::gameobjects::player::Player& player);
};

} // namespace aion::gameserver::network::aion::serverpackets
