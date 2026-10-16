#pragma once

#include "aion/gameserver/handlers/admincommands/AdminCommandsPrelude.h"

namespace aion::gameserver::handlers::admincommands {

/**
 * //useskill: uses (or lets a target use) any skill.
 */
class UseSkill : public AdminCommand {
public:
	UseSkill();

	void execute(Player& player, std::span<const std::string> params) override;

private:
	bool useSkill(Player& player, const SkillTemplate& template_, int32_t skillLevel, std::optional<std::string_view> targetMode, bool forceUse);
	runtime::Ptr<VisibleObject> getTarget(Player& player, std::string_view targetMode);
};

} // namespace aion::gameserver::handlers::admincommands
