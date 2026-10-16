#pragma once

#include "aion/gameserver/handlers/admincommands/AdminCommandsPrelude.h"

namespace aion::gameserver::handlers::admincommands {

/**
 * //delskill: removes skills.
 */
class DelSkill : public AdminCommand {
public:
	DelSkill();

	void execute(Player& player, std::span<const std::string> params) override;

	void apply(Player& admin, Player& player, int32_t skillId, runtime::Ptr<PlayerSkillList> playerSkillList);

	void info(Player& player, std::optional<std::string_view> message) override;

private:
	static bool check(Player& admin, Player& player, int32_t skillId);
};

} // namespace aion::gameserver::handlers::admincommands
