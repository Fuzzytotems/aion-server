#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "regscan_fixture/FakeCore.h"

namespace aion::gameserver::handlers::playercommands {

class Id final : public utils::chathandlers::PlayerCommand {
public:
	Id() : PlayerCommand("id") {}
};
AION_PLAYER_COMMAND(Id);

} // namespace aion::gameserver::handlers::playercommands
