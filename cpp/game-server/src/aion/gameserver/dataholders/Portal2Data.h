#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <unordered_map>

#include "aion/gameserver/dataholders/Portal2Data.xml.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.Portal2Data.
 * <p>
 * C++: the indexes point into the bound lists, which stay after afterUnmarshal (static-data.md §2.6).
 *
 * @author xTz
 */
class Portal2Data : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/Portal2Data.xml.inc"
private:
	std::unordered_map<int32_t, const model::templates::portal::PortalUse*> portalUses;
	std::unordered_map<int32_t, const model::templates::portal::PortalDialog*> portalDialogs;
	std::map<std::string, const model::templates::portal::PortalScroll*, std::less<>> portalScrolls;

public:
	int32_t size() const;

	/**
	 * Tries to find the portal for the players race, but can return the path of the opposite race if there is no matching one.<br>
	 * With this you're able to send an invalid race error to the player (see {@link PortalService#port(PortalPath, Player, int)}).
	 *
	 * @return PortalPath for the specified dialog action ID, nullptr (Java null) if there is none.
	 */
	const model::templates::portal::PortalPath* getPortalDialogPath(int32_t npcId, int32_t dialogActionId,
	                                                                model::gameobjects::player::Player& player) const;

	/**
	 * Tries to find the portal for the players race, but can return the path of the opposite race if there is no matching one.<br>
	 * With this you're able to send an invalid race error to the player (see {@link PortalService#port(PortalPath, Player, int)}).
	 *
	 * @return PortalPath for the specified dialog action ID, nullptr (Java null) if there is none.
	 */
	const model::templates::portal::PortalPath* getPortalUsePath(int32_t npcId, model::gameobjects::player::Player& player) const;

	bool isPortalNpc(int32_t npcId) const;

	/** @return the portal scroll, nullptr (Java null) if there is none */
	const model::templates::portal::PortalScroll* getPortalScroll(std::string_view name) const;

	int32_t getTeleportDialogId(int32_t npcId) const;
};

} // namespace aion::gameserver::dataholders
