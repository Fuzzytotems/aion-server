#pragma once

#include "aion/gameserver/handlers/admincommands/AdminCommandsPrelude.h"

namespace aion::gameserver::handlers::admincommands {

/**
 * //instance: opens or closes the registration of the periodic pvp instances.
 *
 * @author ViAl, Estrayl
 */
class Instance : public AdminCommand {
public:
	Instance();

	void execute(Player& player, std::span<const std::string> params) override;

private:
	void openRegistration(Player& admin, const std::string& instanceName);

	void closeRegistration(Player& admin, const std::string& instanceName);

	static std::string agtName(int32_t maskId);
};

} // namespace aion::gameserver::handlers::admincommands
