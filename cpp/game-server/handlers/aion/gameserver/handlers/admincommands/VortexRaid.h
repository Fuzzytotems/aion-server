#pragma once

#include <span>
#include <string>

#include "aion/gameserver/handlers/admincommands/AdminCommandsPrelude.h"

namespace aion::gameserver::handlers::admincommands {

/**
 * //vortexraid: starts/stops a raid in Theobomos or Brusthonin.
 * <p>
 * Java: data/handlers/admincommands/VortexRaid.java. M5i I-10, under P5-12b2's lease of C1's file.
 */
class VortexRaid : public AdminCommand {
public:
	VortexRaid();

	void execute(Player& player, std::span<const std::string> params) override;
};

} // namespace aion::gameserver::handlers::admincommands
