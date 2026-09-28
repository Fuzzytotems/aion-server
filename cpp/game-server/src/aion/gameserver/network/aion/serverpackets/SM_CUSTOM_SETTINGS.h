#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author Sweetkr
 */
class SM_CUSTOM_SETTINGS : public AionServerPacket {
public:
	static constexpr int32_t HIDE_LEGION_CLOAK = 1;
	static constexpr int32_t HIDE_LEGION_CLOAK_BY_WEAPON_PRIORITY = 2;
	static constexpr int32_t HIDE_HELMET = 4;
	static constexpr int32_t HIDE_PLUME = 8;

private:
	int32_t objectId{};
	int32_t unk{0};
	int32_t display{};
	int32_t deny{};

public:
	explicit SM_CUSTOM_SETTINGS(model::gameobjects::player::Player& player);
	SM_CUSTOM_SETTINGS(int32_t objectId, int32_t unk, int32_t display, int32_t deny);

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
