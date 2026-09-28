#pragma once

#include <cstdint>
#include <string>

#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * Packet about player movement.
 *
 * @author -Nemesiss-
 */
class CM_MOVE : public AionClientPacket {
private:
	int8_t type{};
	int8_t heading{};
	float x{};
	float y{};
	float z{};
	float x2{};
	float y2{};
	float z2{};
	float vehicleX{};
	float vehicleY{};
	float vehicleZ{};
	float vectorX{};
	float vectorY{};
	float vectorZ{};
	int8_t glideFlag{};
	int32_t unk1{};
	int32_t unk2{};
	int32_t geyserLocationId{};

public:
	CM_MOVE(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;

private:
	bool handleBogusPacket(model::gameobjects::player::Player& player);
	void notifyControllers(model::gameobjects::player::Player& player, int8_t oldMovementMask);

public:
	std::string toString() const override;
};

} // namespace aion::gameserver::network::aion::clientpackets
