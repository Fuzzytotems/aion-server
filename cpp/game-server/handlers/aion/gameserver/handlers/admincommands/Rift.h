#pragma once

#include <cstdint>
#include <set>
#include <string>

#include "aion/gameserver/handlers/admincommands/AdminCommandsPrelude.h"

namespace aion::gameserver::handlers::admincommands {

/**
 * //rift: opens or closes rifts in the world.
 * <p>
 * Java: data/handlers/admincommands/Rift.java. M5i I-5. C++-dev only: open all and close all (docs/deviations/P5-13.md).
 */
class Rift : public AdminCommand {
public:
	Rift();

	void execute(Player& player, std::span<const std::string> params) override;

private:
	/** @throws IllegalArgumentException "Invalid rift world ID or location ID." (NumberFormatException for a non-number) */
	int32_t parseId(const std::string& idParam);

	/** The world ids of every rift location, in ascending order. */
	static std::set<int32_t> riftWorlds();
};

} // namespace aion::gameserver::handlers::admincommands
