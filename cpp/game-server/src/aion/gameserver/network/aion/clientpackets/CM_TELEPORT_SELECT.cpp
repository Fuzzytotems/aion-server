#include "aion/gameserver/network/aion/clientpackets/CM_TELEPORT_SELECT.h"

#include <string>

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/animations/TeleportAnimation.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/teleport/TeleLocIdData.h"
#include "aion/gameserver/model/templates/teleport/TeleportLocation.h"
#include "aion/gameserver/model/templates/teleport/TeleporterTemplate.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/audit/AuditLogger.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

using model::gameobjects::Npc;
using model::gameobjects::VisibleObject;
using model::gameobjects::player::Player;
using services::teleport::TeleportService;
using utils::PacketSendUtility;
using utils::audit::AuditLogger;

CM_TELEPORT_SELECT::CM_TELEPORT_SELECT(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_TELEPORT_SELECT.java:38-43
void CM_TELEPORT_SELECT::readImpl() {
	targetObjId = readD();
	locId = readD(); // locationId
	readH();
}

// Java CM_TELEPORT_SELECT.java:45-69. m5f-plan.md D7: the dialog's Daeva check (DialogService.java:188-195) is not repeated here - Java's quirk,
// kept.
void CM_TELEPORT_SELECT::runImpl() {
	const runtime::Ptr<Player> player = getConnection()->getActivePlayer();
	if (player->isDead())
		return;

	runtime::Ptr<VisibleObject> obj = player->getKnownList().getObject(targetObjId);
	const runtime::Ptr<Npc> npc = runtime::as<Npc>(obj);
	if (!npc) {
		if (!obj)
			obj = world::World::getInstance().findVisibleObject(targetObjId);
		AuditLogger::log(*player, "tried to teleport to locId " + std::to_string(locId) + " via "
									  + (!obj ? "unknown npc (objId " + std::to_string(targetObjId) + ")" : obj->toString()) + " at "
									  + player->getPosition()->toString());
		return;
	}
	const model::templates::teleport::TeleporterTemplate* template_ = TeleportService::validateTeleporterAndGetTemplate(*player, *npc);
	if (template_ == nullptr)
		return;
	// Java `template.getTeleLocIdData().getTeleportLocation(locId)`: a template without <locations> is a NullPointerException, and so is one
	// whose <locations> has no <telelocation> - the for-each of getTeleportLocation runs over the null list JAXB leaves, which binds as an
	// empty C++ list (docs/deviations/P5-03.md, the BufEffect row)
	if (template_->getTeleLocIdData() == nullptr)
		throw runtime::NullPointerException("TeleporterTemplate.getTeleLocIdData() of teleporter " + std::to_string(template_->getTeleportId()));
	if (template_->getTeleLocIdData()->getTelelocations().empty())
		throw runtime::NullPointerException("TeleLocIdData.locids of teleporter " + std::to_string(template_->getTeleportId()));
	const model::templates::teleport::TeleportLocation* location = template_->getTeleLocIdData()->getTeleportLocation(locId);
	if (location == nullptr) {
		AuditLogger::log(*player, "tried to teleport to invalid locId " + std::to_string(locId) + " via " + npc->toString() + " at "
									  + player->getPosition()->toString());
		PacketSendUtility::sendPacket(*player, serverpackets::SM_SYSTEM_MESSAGE::STR_CANNOT_MOVE_TO_AIRPORT_NO_ROUTE());
		return;
	}
	TeleportService::teleport(*player, location,
		npc->hasStatic() ? model::animations::TeleportAnimation::JUMP_IN_STATUE : model::animations::TeleportAnimation::JUMP_IN);
}

AION_CLIENT_PACKET(CM_TELEPORT_SELECT);

} // namespace aion::gameserver::network::aion::clientpackets
