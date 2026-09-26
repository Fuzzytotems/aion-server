#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/templates/fwd.h"
#include "aion/gameserver/model/templates/portal/fwd.h"
#include "aion/gameserver/services/teleport/fwd.h"
#include "aion/gameserver/world/fwd.h"

namespace aion::gameserver::services::teleport {

/**
 * @author ATracer, xTz
 */
class PortalService {
public:
	static void port(const model::templates::portal::PortalPath* portalPath, model::gameobjects::player::Player& player, model::gameobjects::Npc& npc);
	static void port(const model::templates::portal::PortalPath* portalPath, model::gameobjects::player::Player& player, model::gameobjects::Npc& npc,
		int8_t difficult);
private:
	static bool checkMentor(model::gameobjects::player::Player& player, int32_t mapId);
	static bool checkEnterLevel(model::gameobjects::player::Player& player, model::gameobjects::Npc& npc,
		const model::templates::portal::PortalPath* portalPath, const model::templates::InstanceCooltime* instanceRestrictions);
	static bool checkRace(model::gameobjects::player::Player& player, model::gameobjects::Npc& npc,
		const model::templates::portal::PortalPath* portalPath);
	static bool checkSiegeId(model::gameobjects::player::Player& player, int32_t sigeId);
	static bool checkRank(model::gameobjects::player::Player& player, model::gameobjects::Npc& npc,
		const model::templates::portal::PortalPath* portalPath);
	static bool checkPlayerSize(model::gameobjects::player::Player& player, model::gameobjects::Npc& npc,
		const model::templates::portal::PortalPath* portalPath, int32_t maxPlayers);
	static bool checkTitle(model::gameobjects::player::Player& player, model::gameobjects::Npc& npc,
		const model::templates::portal::PortalPath* portalPath);
	static bool checkQuests(model::gameobjects::player::Player& player, model::gameobjects::Npc& npc,
		const model::templates::portal::PortalPath* portalPath);
	static bool checkAndRemoveRequiredItems(model::gameobjects::player::Player& player, model::gameobjects::Npc& npc,
		const model::templates::portal::PortalPath* portalPath);
	static void port(model::gameobjects::player::Player& requester, const model::templates::portal::PortalLoc* loc, bool reenter, int32_t maxPlayers);
	static void transfer(model::gameobjects::player::Player& player, const model::templates::portal::PortalLoc* loc, world::WorldMapInstance& instance,
		bool reenter);
};

} // namespace aion::gameserver::services::teleport
