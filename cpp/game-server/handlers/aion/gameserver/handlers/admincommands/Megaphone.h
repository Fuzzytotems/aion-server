#pragma once

#include "aion/gameserver/handlers/admincommands/AdminCommandsPrelude.h"

#include <cstdint>
#include <string>
#include <vector>

namespace aion::gameserver::handlers::admincommands {

/**
 * //megaphone: sends a message to the global faction chat.
 * <p>
 * C++: Java's `private static final List<MegaphoneChatColor> colors = collectColors();` (class initialization, when the command handlers load
 * after the static data) is colors(), a function-local static initialized by collectColors() on first use, which is the constructor's
 * colorIds() call at handler registration.
 */
class Megaphone : public AdminCommand {
public:
	Megaphone();

	void execute(Player& admin, std::span<const std::string> params) override;

	/** Java: private record MegaphoneChatColor(int megaphoneItemId, int color); public here for the parity line of Megaphone.cpp */
	struct MegaphoneChatColor {
		const int32_t megaphoneItemId;
		const int32_t color;
	};

private:
	static const std::vector<MegaphoneChatColor>& colors();

	static std::vector<MegaphoneChatColor> collectColors();

	static std::string colorIds();
};

} // namespace aion::gameserver::handlers::admincommands
