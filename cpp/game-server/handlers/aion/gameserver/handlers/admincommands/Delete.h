#pragma once

#include "aion/gameserver/handlers/admincommands/AdminCommandsPrelude.h"

namespace aion::gameserver::handlers::admincommands {

/**
 * //delete: removes a spawn from the world.
 */
class Delete : public AdminCommand {
public:
	Delete();

	void execute(Player& player, std::span<const std::string> params) override;

private:
	bool delete_(Player& admin, VisibleObject& target, bool notifyOnFail);
};

} // namespace aion::gameserver::handlers::admincommands
