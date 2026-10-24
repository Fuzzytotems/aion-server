#pragma once

#include "aion/gameserver/handlers/admincommands/AdminCommandsPrelude.h"

namespace aion::gameserver::handlers::admincommands {

/**
 * //configure: shows and changes config settings.
 * <p>
 * C++: Java reads and writes the config classes' static fields by reflection; here they are the named binds that
 * commons::configuration::ConfigurableProcessor::describe lists (docs/deviations/C2.md, "C2 cp4").
 *
 * @author ATracer, Rolandas, Neon
 */
class Configure : public AdminCommand {
public:
	Configure();

	void execute(Player& admin, std::span<const std::string> params) override;

private:
	static std::map<std::string, std::string> toMap(std::string value);
};

} // namespace aion::gameserver::handlers::admincommands
