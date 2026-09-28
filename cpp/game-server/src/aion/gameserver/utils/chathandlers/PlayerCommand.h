#pragma once

#include <span>
#include <string>
#include <string_view>

#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/utils/chathandlers/ChatCommand.h"

namespace aion::gameserver::utils::chathandlers {

/**
 * Base of the player commands (data/handlers/playercommands, prefix ".").
 * <p>
 * Hub header (docs/design/hub-headers.md). Registered with AION_PLAYER_COMMAND(Class) (HandlerRegistry.h); see ChatCommand for the C++ shapes.
 *
 * @author synchro2, Neon
 */
class PlayerCommand : public ChatCommand {
public:
	static constexpr std::string_view PREFIX = ".";

	/**
	 * @see PlayerCommand(String, String, String)
	 */
	PlayerCommand(std::string_view alias, std::string_view description);

	/**
	 * @see ChatCommand#ChatCommand(String, String, String, String)
	 */
	PlayerCommand(std::string_view alias, std::string_view description, std::string_view syntaxInfo);

	bool validateAccess(model::gameobjects::player::Player& player) override;

	bool process(model::gameobjects::player::Player& player, std::span<const std::string> params) override;
};

} // namespace aion::gameserver::utils::chathandlers
