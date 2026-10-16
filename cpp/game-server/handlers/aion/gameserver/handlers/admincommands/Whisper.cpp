#include "aion/gameserver/handlers/admincommands/Whisper.h"

#include "aion/commons/utils/StringUtils.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(Whisper);

Whisper::Whisper()
	: AdminCommand("whisper", "Enables/disables incoming whispers.",
		  "on - Allows whispers from others.\n"
		  "off - Blocks whispers from others, except from GMs.\n") {
}

// Java Whisper.java:17-31
void Whisper::execute(Player& admin, std::span<const std::string> params) {
	using commons::utils::StringUtils::equalsIgnoreCase;
	if (params.empty()) {
		sendInfo(admin);
		return;
	}

	if (equalsIgnoreCase(params[0], "off")) {
		admin.setCustomState(CustomPlayerState::NO_WHISPERS_MODE);
		sendInfo(admin, "Accepting whispers: OFF");
	} else if (equalsIgnoreCase(params[0], "on")) {
		admin.unsetCustomState(CustomPlayerState::NO_WHISPERS_MODE);
		sendInfo(admin, "Accepting whispers: ON");
	}
}

} // namespace aion::gameserver::handlers::admincommands
