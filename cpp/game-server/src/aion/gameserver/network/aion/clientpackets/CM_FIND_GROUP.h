#pragma once

#include <cstdint>
#include <string>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * The Find Group window: recruitments, applications and instance groups (C_FIND_GROUP).
 *
 * @author cura, MrPoke
 */
class CM_FIND_GROUP : public AionClientPacket {
private:
	int32_t action{};
	int32_t playerOrTeamId{};
	[[maybe_unused]] int32_t bannedPlayerId{}; // read for action 25, never used (Java)
	std::string message;
	int32_t groupType{};
	int32_t classId{};
	int32_t level{};
	int8_t serverId{};
	int8_t unk1{};
	int8_t unk2{};
	int8_t unk3{};
	int32_t instanceMaskId{};
	int32_t minMembers{};
	int8_t instanceApplicationReply{};

public:
	CM_FIND_GROUP(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
