#pragma once

#include <cstdint>
#include <string>

#include "aion/gameserver/handlers/admincommands/AdminCommandsPrelude.h"

namespace aion::gameserver::handlers::admincommands {

/**
 * //rift: opens or closes rifts in the world.
 * <p>
 * Java: data/handlers/admincommands/Rift.java. M5i I-5.
 */
class Rift : public AdminCommand {
public:
	Rift();

	void execute(Player& player, std::span<const std::string> params) override;

private:
	/** @throws IllegalArgumentException "Invalid rift world ID or location ID." (NumberFormatException for a non-number) */
	int32_t parseId(const std::string& idParam);
};

} // namespace aion::gameserver::handlers::admincommands
