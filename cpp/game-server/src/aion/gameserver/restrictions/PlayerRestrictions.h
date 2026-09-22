#pragma once

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/team/fwd.h"
#include "aion/gameserver/restrictions/fwd.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::restrictions {

/**
 * The static restriction checks a player action runs first. Declaration drafted by `skeleton.py --draft` from
 * game-server/src/com/aionemu/gameserver/restrictions/PlayerRestrictions.java (hub-headers.md §5.1 decides the reference and Ptr parameters:
 * `canTrade`, `canChat` and `canUseItem` take a Ptr because Java checks `player == null` in their first statement, the others a reference).
 * <p>
 * M5b-1 (m5b-plan.md C-01) ports `canAttack` and its `checkFly` helper, the first statement of `PlayerController::attackTarget`. `canUseSkill`
 * is an AION_PARTIAL until M5b-2 and the remaining bodies are AION_UNPORTED; see docs/deviations/P5-13.md.
 *
 * @author lord_rex, Sippolo
 */
class PlayerRestrictions {
private:
	static bool checkFly(model::gameobjects::player::Player& player);

public:
	static bool canUseSkill(model::gameobjects::player::Player& player, skillengine::model::Skill& skill);
	static bool canInviteToGroup(model::gameobjects::player::Player& player, model::gameobjects::player::Player& target);
	static bool canInviteToAlliance(model::gameobjects::player::Player& player, model::gameobjects::player::Player& target);

private:
	static bool canInviteToTeam(model::gameobjects::player::Player& player, runtime::Ptr<model::gameobjects::player::Player> target,
		bool isAlliance, runtime::Ptr<model::team::TemporaryPlayerTeam> team);

public:
	static bool canAttack(model::gameobjects::player::Player& player, model::gameobjects::VisibleObject& target);
	static bool canTrade(runtime::Ptr<model::gameobjects::player::Player> player);
	static bool canChat(runtime::Ptr<model::gameobjects::player::Player> player);
	static bool canUseItem(runtime::Ptr<model::gameobjects::player::Player> player, model::gameobjects::Item& item);
	static bool canChangeEquip(model::gameobjects::player::Player& player);
};

} // namespace aion::gameserver::restrictions
