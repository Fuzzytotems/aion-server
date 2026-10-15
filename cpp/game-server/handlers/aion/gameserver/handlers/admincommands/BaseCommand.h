#pragma once

#include <span>
#include <string>

#include "aion/gameserver/handlers/admincommands/AdminCommandsPrelude.h"

namespace aion::gameserver::handlers::admincommands {

/**
 * //base: lists bases or changes their state.
 * <p>
 * Java: data/handlers/admincommands/BaseCommand.java. M5i I-8.
 */
class BaseCommand : public AdminCommand {
public:
	BaseCommand();

	void execute(Player& player, std::span<const std::string> params) override;

private:
	void showBaseLocationList(Player& player);

	void startBase(Player& player, std::span<const std::string> params);

	void stopBase(Player& player, std::span<const std::string> params);

protected:
	void captureBase(Player& player, std::span<const std::string> params);

	void assaultBase(Player& player, std::span<const std::string> params);

private:
	int32_t parseBaseId(Player& admin, std::span<const std::string> params, int32_t expectedParameters);
};

} // namespace aion::gameserver::handlers::admincommands
