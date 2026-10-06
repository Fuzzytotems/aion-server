#pragma once

#include "aion/gameserver/handlers/admincommands/AdminCommandsPrelude.h"

#include "aion/gameserver/model/stats/calc/StatOwner.h"

namespace aion::gameserver::handlers::admincommands {

/**
 * //speed: sets your speed. Java: AdminCommand implements StatOwner - the command object (immortal once registered) owns the speed functions.
 */
class Speed : public AdminCommand, public model::stats::calc::StatOwner {
public:
	Speed();

	void execute(Player& player, std::span<const std::string> params) override;

	/** a registered command is immortal (ChatProcessor keeps it for the process): Ref<StatOwner> may hold it */
	void retain() const noexcept override {}
	void release() const noexcept override {}
};

} // namespace aion::gameserver::handlers::admincommands
