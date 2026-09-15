#include "aion/gameserver/network/aion/serverpackets/SM_INSTANCE_INFO.h"

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/InstanceCooltimeData.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PortalCooldown.h"
#include "aion/gameserver/model/gameobjects/player/PortalCooldownList.h"
#include "aion/gameserver/model/templates/InstanceCooltime.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::serverpackets {

namespace {

/** Java dereferences the cooltime template directly: NullPointerException for a world without one */
const model::templates::InstanceCooltime& requireCooltime(const model::templates::InstanceCooltime* cooltime) {
	if (cooltime == nullptr)
		throw runtime::NullPointerException("InstanceCooltime is null");
	return *cooltime;
}

} // namespace

SM_INSTANCE_INFO::SM_INSTANCE_INFO(int8_t updateTypeValue, model::gameobjects::player::Player& player, std::initializer_list<int32_t> instanceId)
	: SM_INSTANCE_INFO(updateTypeValue, std::vector<runtime::Ptr<model::gameobjects::player::Player>>{player}, instanceId) {
}

SM_INSTANCE_INFO::SM_INSTANCE_INFO(int8_t updateTypeValue, const std::vector<runtime::Ptr<model::gameobjects::player::Player>>& playersValue,
	std::initializer_list<int32_t> instanceId)
	: AionServerPacket(opcodeOf<SM_INSTANCE_INFO>), updateType(updateTypeValue), players(playersValue.begin(), playersValue.end()) {
	if (instanceId.size() > 0) {
		instanceIds.assign(instanceId.begin(), instanceId.end());
	} else {
		// Java: DataManager.INSTANCE_COOLTIME_DATA.getInstanceCooltimes().keySet() (the holder returns the keys in the LinkedHashMap's insertion
		// order, the XML order)
		for (const auto& [worldId, cooltime] : dataholders::DataManager::INSTANCE_COOLTIME_DATA->getInstanceCooltimes())
			instanceIds.push_back(worldId);
	}
}

SM_INSTANCE_INFO::~SM_INSTANCE_INFO() = default;

void SM_INSTANCE_INFO::writeImpl(AionConnection* con) {
	if (con == nullptr)
		throw runtime::NullPointerException("SM_INSTANCE_INFO::writeImpl without a connection");
	const dataholders::InstanceCooltimeData& cooltimeData = *dataholders::DataManager::INSTANCE_COOLTIME_DATA;
	runtime::Ptr<model::gameobjects::player::Player> activePlayer = con->getActivePlayer();
	writeC(updateType);
	// cooldown ID if only one instance is updated
	writeD(instanceIds.size() == 1 ? requireCooltime(cooltimeData.getInstanceCooltimeByWorldId(instanceIds[0])).getId() : 0);
	writeC(0x00); // unk1
	writeH(static_cast<int32_t>(players.size()));
	for (const runtime::Ref<model::gameobjects::player::Player>& player : players) {
		writeD(player->getObjectId());
		writeH(static_cast<int32_t>(instanceIds.size()));
		for (int32_t worldId : instanceIds) {
			runtime::Ptr<model::gameobjects::player::PortalCooldown> cooldown = player->getPortalCooldownList().getPortalCooldown(worldId);
			const model::templates::InstanceCooltime& cooltime = requireCooltime(cooltimeData.getInstanceCooltimeByWorldId(worldId));
			writeD(cooltime.getId());
			writeD(0x00);
			// will only be shown from client if entriesUsed == maxEntries; Java: (int) (reuseTime - now) / 1000
			writeD(!cooldown ? 0 : static_cast<int32_t>(cooldown->getReuseTime() - commons::utils::currentTimeMillis()) / 1000);
			writeD(cooltime.getMaxCount()); // max entries
			writeD(!cooldown ? 0 : -cooldown->getEnterCount()); // entry offset (from max)
			writeC(cooltime.getRace() == activePlayer->getOppositeRace() ? 0 : 1); // hide flag (1 = show, 0 = hide instance from list)
		}
		writeS(player->getName());
	}
}

} // namespace aion::gameserver::network::aion::serverpackets
