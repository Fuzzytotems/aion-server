#pragma once

#include "aion/gameserver/handlers/admincommands/AdminCommandsPrelude.h"

namespace aion::gameserver::handlers::admincommands {

/**
 * //collide: geo debugging tool, lists the collisions between the target and the ground (or the GM).
 *
 * @author Rolandas
 */
class Collide : public AdminCommand {
public:
	Collide();

	void execute(Player& admin, std::span<const std::string> params) override;

private:
	void listCollisions(Player& admin, geoEngine::collision::CollisionResults& results, const geoEngine::collision::CollisionResult& closestOpposite);
};

} // namespace aion::gameserver::handlers::admincommands
