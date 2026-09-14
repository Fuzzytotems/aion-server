#pragma once

#include <span>
#include <string>
#include <string_view>

#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/utils/chathandlers/ChatCommand.h"

namespace aion::commons::logging {
class Logger;
} // namespace aion::commons::logging

namespace aion::gameserver::utils::chathandlers {

/**
 * Base of the console commands (data/handlers/consolecommands, no prefix).
 * <p>
 * Hub header (docs/design/hub-headers.md). Registered with AION_CONSOLE_COMMAND(Class) (HandlerRegistry.h); see ChatCommand for the C++
 * shapes. The package-private audit logger is a class member (hub-headers.md §11.3).
 *
 * @author ginho1, Neon
 */
class ConsoleCommand : public ChatCommand {
public:
	static constexpr std::string_view PREFIX = "";
	/** Java: static final Logger log = LoggerFactory.getLogger("ADMINAUDIT_LOG") */
	static const commons::logging::Logger log;

	/** only for backwards compatibility (Java: TODO remove when all commands are updated) */
	explicit ConsoleCommand(std::string_view alias);

	/**
	 * @see ConsoleCommand(String, String, String)
	 */
	ConsoleCommand(std::string_view alias, std::string_view description);

	/**
	 * Creates a new console command for use with the GM Panel (Shift + F1) or in macros if the console has been activated via
	 * {@code \con_disable_console 0} from the command tab of the GM Panel.
	 *
	 * @see ChatCommand#ChatCommand(String, String, String, String)
	 */
	ConsoleCommand(std::string_view alias, std::string_view description, std::string_view syntaxInfo);

	bool validateAccess(model::gameobjects::player::Player& player) override;

	bool process(model::gameobjects::player::Player& player, std::span<const std::string> params) override;
};

} // namespace aion::gameserver::utils::chathandlers
