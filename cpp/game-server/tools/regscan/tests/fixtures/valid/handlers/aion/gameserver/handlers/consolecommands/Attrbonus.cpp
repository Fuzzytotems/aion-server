#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "regscan_fixture/FakeCore.h"

namespace aion::gameserver::handlers::consolecommands {

class Attrbonus final : public utils::chathandlers::ConsoleCommand {
public:
	Attrbonus() : ConsoleCommand("attrbonus") {}
};
AION_CONSOLE_COMMAND(Attrbonus);

} // namespace aion::gameserver::handlers::consolecommands
