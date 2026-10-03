#include "aion/gameserver/network/aion/clientpackets/CM_CRAFT.h"

#include "aion/gameserver/GameServer.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/VisibleObjectTemplate.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/craft/CraftService.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

using model::gameobjects::VisibleObject;
using model::gameobjects::player::Player;

CM_CRAFT::CM_CRAFT(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_CRAFT.java:32-41
void CM_CRAFT::readImpl() {
	unk = readUC();
	targetTemplateId = readD();
	recipeId = readD();
	targetObjId = readD();
	const int32_t materialsCount = readUH();
	craftType = readUC();
	for (int32_t i = 0; i < materialsCount; i++) {
		// Java materialsData.put(readD(), readQ()): the item id is read before the count (Java evaluates arguments left to right; a C++
		// `map[readD()] = readQ()` would read the count first), and a repeated item id keeps the count read last
		const int32_t itemId = readD();
		const int64_t count = readQ();
		materialsData.insert_or_assign(itemId, count);
	}
}

// Java CM_CRAFT.java:44-61
void CM_CRAFT::runImpl() {
	const runtime::Ptr<Player> player = getConnection()->getActivePlayer();

	if (!player || !player->isSpawned())
		return;
	if (GameServer::isShuttingDownSoon()) // stop crafting to avoid unnecessary material loss
		return;

	// 129 = Morph Substances
	if (unk != 129) {
		const runtime::Ptr<VisibleObject> staticObject = player->getKnownList().getObject(targetObjId);
		if (!staticObject || !utils::PositionUtil::isInRange(*player, *staticObject, 10))
			return;
		// Java dereferences getObjectTemplate() unchecked: an object without a template is its NullPointerException
		const model::templates::VisibleObjectTemplate* objectTemplate = staticObject->getObjectTemplate();
		if (objectTemplate == nullptr)
			throw runtime::NullPointerException(
				"Cannot invoke \"VisibleObjectTemplate.getTemplateId()\" because the return value of \"VisibleObject.getObjectTemplate()\" is null");
		if (objectTemplate->getTemplateId() != targetTemplateId)
			return;
	}

	services::craft::CraftService::startCrafting(*player, recipeId, targetObjId, craftType, materialsData);
}

AION_CLIENT_PACKET(CM_CRAFT);

} // namespace aion::gameserver::network::aion::clientpackets
