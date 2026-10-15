#pragma once

#include <optional>
#include <string>
#include <vector>

#include "aion/gameserver/handlers/admincommands/AdminCommandsPrelude.h"

namespace aion::gameserver::handlers::admincommands {

/**
 * //sysmail: sends a system mail (items, kinah) to a player, a race or everyone online.
 */
class SysMail : public AdminCommand {
public:
	SysMail();

	void execute(Player& admin, std::span<const std::string> params) override;

	void info(Player& player, std::optional<std::string_view> message) override;

private:
	enum class RecipientType { ELYOS, ASMO, ALL, PLAYER };

	/** Java: RecipientType.isAllowed(race) */
	static bool isAllowed(RecipientType type, Race race);

	/** Java: the enum constant's name (string conversion) */
	static std::string recipientTypeName(RecipientType type);

	/** Java: extractText(String[] words, String[] outText) - outText is a one-element array */
	int32_t extractText(const std::vector<std::string>& words, std::vector<std::string>& outText);

	static bool checkExpress(Player& admin, int32_t item, int32_t count, int32_t kinah, const std::optional<std::string>& recipient,
		std::optional<RecipientType> recipientType, LetterType letterType);
};

} // namespace aion::gameserver::handlers::admincommands
