#include "aion/gameserver/dataholders/Portal2Data.h"

#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"

namespace aion::gameserver::dataholders {

using model::templates::portal::PortalDialog;
using model::templates::portal::PortalPath;
using model::templates::portal::PortalScroll;
using model::templates::portal::PortalUse;

void Portal2Data::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	for (const PortalUse& portal : portalUse) // Java: if (portalUse != null)
		portalUses.insert_or_assign(portal.getNpcId(), &portal);
	for (const PortalDialog& portal : portalDialog)
		portalDialogs.insert_or_assign(portal.getNpcId(), &portal);
	for (const PortalScroll& portal : portalScroll)
		portalScrolls.insert_or_assign(portal.getName(), &portal);
	// Java: portalUse = portalDialog = portalScroll = null (the C++ indexes point into the storage, which stays)
}

int32_t Portal2Data::size() const {
	return static_cast<int32_t>(portalScrolls.size() + portalDialogs.size() + portalUses.size());
}

const PortalPath* Portal2Data::getPortalDialogPath(int32_t npcId, int32_t dialogActionId, model::gameobjects::player::Player& player) const {
	auto portal = portalDialogs.find(npcId);
	if (portal != portalDialogs.end()) {
		const PortalPath* matchingPortalPath = nullptr;
		for (const PortalPath& path : portal->second->getPortalPaths()) {
			if (path.getDialog() == dialogActionId) {
				if (path.getRace() == player.getRace() || path.getRace() == model::Race::PC_ALL)
					return &path;
				matchingPortalPath = &path;
			}
		}
		return matchingPortalPath; // return any matched path to send invalid race error afterwards
	}
	return nullptr;
}

const PortalPath* Portal2Data::getPortalUsePath(int32_t npcId, model::gameobjects::player::Player& player) const {
	auto portal = portalUses.find(npcId);
	if (portal != portalUses.end()) {
		const PortalPath* matchingPortalPath = nullptr;
		for (const PortalPath& path : portal->second->getPortalPaths()) {
			if (player.getRace() == path.getRace() || path.getRace() == model::Race::PC_ALL)
				return &path;
			matchingPortalPath = &path;
		}
		return matchingPortalPath; // return any matched path to send invalid race error afterwards
	}
	return nullptr;
}

bool Portal2Data::isPortalNpc(int32_t npcId) const {
	return portalUses.contains(npcId) || portalDialogs.contains(npcId);
}

const PortalScroll* Portal2Data::getPortalScroll(std::string_view name) const {
	auto it = portalScrolls.find(name);
	return it != portalScrolls.end() ? it->second : nullptr;
}

int32_t Portal2Data::getTeleportDialogId(int32_t npcId) const {
	auto portal = portalDialogs.find(npcId);
	return portal == portalDialogs.end() ? 1011 : portal->second->getTeleportDialogId();
}

} // namespace aion::gameserver::dataholders
