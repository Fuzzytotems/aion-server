#include <cstdint>

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "regscan_fixture/FakeCore.h"

namespace aion::gameserver::network::aion::clientpackets {

namespace {
// core sources may use anonymous namespaces and namespace-scope statics (the unity rules apply to handler files only)
constexpr int32_t MOVE_OPCODE = 48;
} // namespace

static int32_t opcodeOfMove() {
	return MOVE_OPCODE;
}

class CM_MOVE final : public AionClientPacket {
public:
	CM_MOVE(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode == 0 ? opcodeOfMove() : opcode, validStates) {}
};
AION_CLIENT_PACKET(CM_MOVE);

} // namespace aion::gameserver::network::aion::clientpackets
