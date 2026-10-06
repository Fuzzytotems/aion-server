#include "aion/gameserver/handlers/admincommands/Delete.h"

#include <typeinfo>

#include "aion/commons/configuration/transformers/NumberTransformer.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/controllers/VisibleObjectController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/SpawnsData.h"
#include "aion/gameserver/model/gameobjects/Gatherable.h"
#include "aion/gameserver/model/gameobjects/HouseObject.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/utils/SimpleClassName.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(Delete);

Delete::Delete()
	: AdminCommand("delete", "Removes a spawn from world.",
		  " - Deletes the object you are targeting.\n"
		  "<range> - Deletes all objects around you in given radius in meters.\n") {
}

// Java Delete.java:30-46
void Delete::execute(Player& admin, std::span<const std::string> params) {
	if (params.empty()) {
		if (admin.getTarget() == nullptr)
			sendInfo(admin);
		else
			delete_(admin, *admin.getTarget(), true);
	} else {
		int32_t count = 0; // Java: int[] count = { 0 }
		float range = commons::configuration::transformers::NumberParser::parseFloat(params[0]); // Java: Float.parseFloat
		admin.getKnownList().forEachObject([this, &admin, &count, range](VisibleObject& object) {
			if (PositionUtil::isInRange(admin, object, range) && delete_(admin, object, false))
				count++;
		});
		sendInfo(admin, "Deleted " + std::to_string(count) + (count == 1 ? " object." : " objects."));
	}
}

// Java Delete.java:48-73. C++: delete_ (Java delete; `delete` is a keyword)
bool Delete::delete_(Player& admin, VisibleObject& target, bool notifyOnFail) {
	if (dynamic_cast<Npc*>(&target) == nullptr && dynamic_cast<Gatherable*>(&target) == nullptr && dynamic_cast<HouseObject*>(&target) == nullptr) {
		if (notifyOnFail)
			PacketSendUtility::sendPacket(admin, SM_SYSTEM_MESSAGE::STR_INVALID_TARGET());
		return false;
	}
	runtime::Ptr<SpawnTemplate> spawn = target.getSpawn();
	if (spawn != nullptr) { // house objects have no spawn template
		if (spawn->hasPool()) {
			if (notifyOnFail)
				sendInfo(admin, "Can't delete pooled spawn template.");
			return false;
		}
		if (typeid(*spawn) != typeid(SpawnTemplate)) {
			if (notifyOnFail)
				sendInfo(admin, "Can't delete special spawns (spawn type: " +
									commons::utils::StringUtils::replace(utils::simpleClassName(typeid(*spawn)), "Template", "") + ").");
			return false;
		}
	}
	target.getController().delete_();
	if (DataManager::SPAWNS_DATA->saveSpawn(target, true))
		sendInfo(admin, "Spawn removed permanently. " + utils::simpleClassName(typeid(target)) + " will not spawn on server start anymore.");
	return true;
}

} // namespace aion::gameserver::handlers::admincommands
