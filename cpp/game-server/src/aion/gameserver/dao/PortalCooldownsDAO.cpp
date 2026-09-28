#include "aion/gameserver/dao/PortalCooldownsDAO.h"

#include <string_view>

#include "aion/commons/database/DatabaseFactory.h"
#include "aion/commons/database/PreparedStatement.h"
#include "aion/commons/database/ResultSet.h"
#include "aion/commons/database/SQLException.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PortalCooldown.h"
#include "aion/gameserver/model/gameobjects/player/PortalCooldownList.h"
#include "aion/gameserver/runtime/collections/HashMap.h"
#include "aion/gameserver/runtime/collections/Rc.h"

namespace aion::gameserver::dao {

using commons::database::DatabaseFactory;
using commons::database::SQLException;
using model::gameobjects::player::PortalCooldown;

namespace {

constexpr std::string_view INSERT_QUERY = "INSERT INTO `portal_cooldowns` (`player_id`, `world_id`, `reuse_time`, `entry_count`) VALUES (?,?,?,?)";
constexpr std::string_view DELETE_QUERY = "DELETE FROM `portal_cooldowns` WHERE `player_id`=?";
constexpr std::string_view SELECT_QUERY = "SELECT `world_id`, `reuse_time`, `entry_count` FROM `portal_cooldowns` WHERE `player_id`=?";

} // namespace

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.PortalCooldownsDAO");

void PortalCooldownsDAO::loadPortalCooldowns(model::gameobjects::player::Player& player) {
	runtime::Ref<runtime::RcHashMap<int32_t, runtime::Ref<PortalCooldown>>> portalCoolDowns =
		runtime::RcHashMap<int32_t, runtime::Ref<PortalCooldown>>::create();
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(SELECT_QUERY);
		stmt->setInt(1, player.getObjectId());
		auto rset = stmt->executeQuery();
		while (rset->next()) {
			int32_t worldId = rset->getInt("world_id");
			int64_t reuseTime = rset->getLong("reuse_time");
			int32_t entryCount = rset->getInt("entry_count");
			if (reuseTime > commons::utils::currentTimeMillis()) {
				portalCoolDowns->put(worldId, PortalCooldown::create(worldId, reuseTime, entryCount));
			}
		}
		player.getPortalCooldownList().setPortalCoolDowns(portalCoolDowns);
	} catch (const SQLException& e) {
		log.error("LoadPortalCooldowns", e);
	}
}

void PortalCooldownsDAO::storePortalCooldowns(model::gameobjects::player::Player& player) {
	deletePortalCooldowns(player);
	runtime::Ptr<runtime::RcHashMap<int32_t, runtime::Ref<PortalCooldown>>> portalCoolDowns = player.getPortalCooldownList().getPortalCoolDowns();
	if (!portalCoolDowns)
		return;

	for (const auto& entry : portalCoolDowns->snapshot()) {
		int32_t worldId = entry.getKey();
		int64_t reuseTime = entry.getValue()->getReuseTime();
		int32_t entryCount = entry.getValue()->getEnterCount();
		if (reuseTime < commons::utils::currentTimeMillis())
			continue;
		try {
			auto con = DatabaseFactory::getConnection();
			auto stmt = con->prepareStatement(INSERT_QUERY);
			stmt->setInt(1, player.getObjectId());
			stmt->setInt(2, worldId);
			stmt->setLong(3, reuseTime);
			stmt->setInt(4, entryCount);
			stmt->execute();
		} catch (const SQLException& e) {
			log.error("storePortalCooldowns", e);
		}
	}
}

void PortalCooldownsDAO::deletePortalCooldowns(model::gameobjects::player::Player& player) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(DELETE_QUERY);
		stmt->setInt(1, player.getObjectId());
		stmt->execute();
	} catch (const SQLException& e) {
		log.error("deletePortalCooldowns", e);
	}
}

} // namespace aion::gameserver::dao
