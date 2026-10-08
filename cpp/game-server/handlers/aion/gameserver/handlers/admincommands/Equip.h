#pragma once

#include "aion/gameserver/handlers/admincommands/AdminCommandsPrelude.h"

namespace aion::gameserver::handlers::admincommands {

/**
 * //equip: sockets, unsockets, enchants or tempers all equipped items.
 */
class Equip : public AdminCommand {
public:
	Equip();

	void execute(Player& player, std::span<const std::string> params) override;

private:
	void socket(Player& admin, Player& player, const ItemTemplate* manastone, int32_t count);

	void unsocket(Player& admin, Player& player);

	void enchant(Player& admin, Player& player, int32_t enchant);

	void temper(Player& admin, Player& player, int32_t temperingLevel);
};

} // namespace aion::gameserver::handlers::admincommands
