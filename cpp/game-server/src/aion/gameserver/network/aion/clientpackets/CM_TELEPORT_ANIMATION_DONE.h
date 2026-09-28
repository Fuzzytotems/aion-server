#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * Sent by the client when the teleport animation is done and the actual teleport should be executed.
 * <p>
 * C++: the TELEPORT task TeleportService::sendLoc stores is a `Future::deferred` (Java: a FutureTask bound to no executor), so the body runs on
 * this packet's thread through Future::run, and a stored exception comes back from Future::get as an ExecutionException (Future.h).
 *
 * @author Rolandas, Neon
 */
class CM_TELEPORT_ANIMATION_DONE : public AionClientPacket {
public:
	CM_TELEPORT_ANIMATION_DONE(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
