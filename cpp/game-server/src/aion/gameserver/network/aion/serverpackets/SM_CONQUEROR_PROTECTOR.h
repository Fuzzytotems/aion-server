#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * SM_SERIAL_KILLER pre 4.8
 *
 * @author Source, xTz
 */
class SM_CONQUEROR_PROTECTOR : public AionServerPacket {
private:
	int32_t type{};
	int32_t buffLvl{};
	int32_t cooldown{};
	runtime::Ref<model::gameobjects::player::Player> player{};
	std::vector<runtime::Ref<model::gameobjects::player::Player>> intruders{};

public:
	SM_CONQUEROR_PROTECTOR(int32_t type, int32_t buffLvl, int32_t cooldown);
	SM_CONQUEROR_PROTECTOR(int32_t type, int32_t buffLvl);
	SM_CONQUEROR_PROTECTOR(int32_t type, model::gameobjects::player::Player& player);
	SM_CONQUEROR_PROTECTOR(const std::vector<runtime::Ptr<model::gameobjects::player::Player>>& intruders, bool displayCd);
	~SM_CONQUEROR_PROTECTOR() override;

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
