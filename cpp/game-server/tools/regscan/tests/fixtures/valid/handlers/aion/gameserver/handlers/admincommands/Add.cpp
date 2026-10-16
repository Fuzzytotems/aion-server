#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "regscan_fixture/FakeCore.h"

namespace aion::gameserver::handlers::admincommands {

class Add final : public utils::chathandlers::AdminCommand {
public:
	Add() : AdminCommand("add") {}
};
AION_ADMIN_COMMAND(Add);

} // namespace aion::gameserver::handlers::admincommands
