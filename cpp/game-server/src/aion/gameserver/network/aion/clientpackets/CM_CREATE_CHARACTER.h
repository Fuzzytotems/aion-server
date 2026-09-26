#pragma once

#include <cstdint>

#include "aion/gameserver/model/account/fwd.h"
#include "aion/gameserver/network/aion/clientpackets/AbstractCharacterEditPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * In this packets the Aion client is requesting creation of a character or to enter the character creation menu.
 * <p>
 * C++: the transient Player that PlayerService.newPlayer creates is only held by a local Ref, so it is released on every exit (success,
 * validation error, database error).
 *
 * @author -Nemesiss-, cura, Neon
 */
class CM_CREATE_CHARACTER : public AbstractCharacterEditPacket {
private:
	int32_t type{};

public:
	CM_CREATE_CHARACTER(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;

private:
	int32_t validateBasicInfo(model::account::Account& account);
};

} // namespace aion::gameserver::network::aion::clientpackets
