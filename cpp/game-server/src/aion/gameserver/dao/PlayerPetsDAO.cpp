#include "aion/gameserver/dao/PlayerPetsDAO.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "aion/commons/database/DatabaseFactory.h"
#include "aion/commons/database/PreparedStatement.h"
#include "aion/commons/database/ResultSet.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Numbers.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/dao/detail/DaoSupport.h"
#include "aion/gameserver/dao/detail/UsedIds.h"
#include "aion/gameserver/model/gameobjects/player/PetCommonData.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/pet/PetDopingBag.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/toypet/PetFeedProgress.h"
#include "aion/gameserver/services/toypet/PetHungryLevel.h"

namespace aion::gameserver::dao {

using commons::database::DatabaseFactory;
using model::gameobjects::player::PetCommonData;

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.PlayerPetsDAO");

namespace {

/** Java PetHungryLevel.fromId(value): values()[value] */
services::toypet::PetHungryLevel petHungryLevelFromId(int32_t value) {
	constexpr int32_t count = 4; // HUNGRY, CONTENT, SEMIFULL, FULL
	if (value < 0 || value >= count)
		throw runtime::ArrayIndexOutOfBoundsException("Index " + std::to_string(value) + " out of bounds for length " + std::to_string(count));
	return static_cast<services::toypet::PetHungryLevel>(value);
}

} // namespace

void PlayerPetsDAO::saveFeedStatus(int32_t petObjectId, int32_t hungryLevel, int32_t feedProgress, int64_t reuseTime) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement("UPDATE player_pets SET hungry_level = ?, feed_progress = ?, reuse_time = ? WHERE id = ?");
		stmt->setInt(1, hungryLevel);
		stmt->setInt(2, feedProgress);
		stmt->setLong(3, reuseTime);
		stmt->setInt(4, petObjectId);
		stmt->execute();
	} catch (const std::exception& e) {
		log.error("Error updating feed status for pet #" + std::to_string(petObjectId), e);
	}
}

void PlayerPetsDAO::saveDopingBag(int32_t petObjectId, model::templates::pet::PetDopingBag& bag) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement("UPDATE player_pets SET dopings = ? WHERE id = ?");
		std::string itemIds = std::to_string(bag.getFoodItem()) + "," + std::to_string(bag.getDrinkItem());
		for (int32_t itemId : bag.getScrollsUsed())
			itemIds += "," + std::to_string(itemId);
		stmt->setString(1, itemIds);
		stmt->setInt(2, petObjectId);
		stmt->execute();
	} catch (const std::exception& e) {
		log.error("Error update doping for pet #" + std::to_string(petObjectId), e);
	}
}

void PlayerPetsDAO::setTime(int32_t petObjectId, int64_t time) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement("UPDATE player_pets SET reuse_time = ? WHERE id = ?");
		stmt->setLong(1, time);
		stmt->setInt(2, petObjectId);
		stmt->execute();
	} catch (const std::exception& e) {
		log.error("Error update pet #" + std::to_string(petObjectId), e);
	}
}

void PlayerPetsDAO::insertPlayerPet(model::gameobjects::player::Player& player, model::gameobjects::player::PetCommonData& petCommonData) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(
			"INSERT INTO player_pets(id, player_id, template_id, decoration, name, despawn_time, expire_time) VALUES(?, ?, ?, ?, ?, ?, ?)");
		stmt->setInt(1, petCommonData.getObjectId());
		stmt->setInt(2, player.getObjectId());
		stmt->setInt(3, petCommonData.getTemplateId());
		stmt->setInt(4, petCommonData.getDecoration());
		stmt->setString(5, petCommonData.getName());
		stmt->setTimestamp(6, petCommonData.getDespawnTime());
		stmt->setInt(7, petCommonData.getExpireTime());
		stmt->execute();
	} catch (const std::exception& e) {
		log.error("Error inserting new pet #" + std::to_string(petCommonData.getObjectId()) + ", name: " + petCommonData.getName(), e);
	}
}

void PlayerPetsDAO::removePlayerPet(int32_t petObjectId) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement("DELETE FROM player_pets WHERE id = ?");
		stmt->setInt(1, petObjectId);
		stmt->execute();
	} catch (const std::exception& e) {
		log.error("Error removing pet #" + std::to_string(petObjectId), e);
	}
}

std::vector<runtime::Ref<model::gameobjects::player::PetCommonData>> PlayerPetsDAO::getPlayerPets(model::gameobjects::player::Player& player) {
	std::vector<runtime::Ref<PetCommonData>> pets;
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement("SELECT * FROM player_pets WHERE player_id = ?");
		stmt->setInt(1, player.getObjectId());
		auto rs = stmt->executeQuery();
		while (rs->next()) {
			runtime::Ref<PetCommonData> petCommonData =
				PetCommonData::create(rs->getInt("id"), rs->getInt("template_id"), player.getObjectId(), rs->getInt("expire_time"));
			petCommonData->setName(rs->getString("name"));
			petCommonData->setDecoration(rs->getInt("decoration"));
			if (petCommonData->getFeedProgress()) {
				petCommonData->getFeedProgress()->setHungryLevel(petHungryLevelFromId(rs->getInt("hungry_level")));
				petCommonData->getFeedProgress()->setData(rs->getInt("feed_progress"));
				petCommonData->setRefeedTime(rs->getLong("reuse_time"));
			}
			if (petCommonData->getDopingBag()) {
				std::optional<std::string> dopings = rs->getObject<std::string>("dopings");
				if (dopings) {
					std::vector<std::string_view> ids = detail::javaSplit(*dopings, ',');
					for (size_t i = 0; i < ids.size(); i++)
						petCommonData->getDopingBag()->setItem(commons::utils::parseInt(ids[i]), static_cast<int32_t>(i));
				}
			}
			petCommonData->setBirthday(rs->getTimestamp("birthday"));
			petCommonData->setStartMoodTime(rs->getLong("mood_started"));
			petCommonData->setShuggleCounter(rs->getInt("counter"));
			petCommonData->setMoodCdStarted(rs->getLong("mood_cd_started"));
			petCommonData->setGiftCdStarted(rs->getLong("gift_cd_started"));
			std::optional<commons::database::Timestamp> ts = rs->getTimestamp("despawn_time");
			if (!ts)
				ts = detail::toTimestamp(commons::utils::currentTimeMillis());
			petCommonData->setDespawnTime(ts);
			pets.push_back(std::move(petCommonData));
		}
	} catch (const std::exception& e) {
		log.error("Error loading pets for " + player.toString(), e);
	}
	return pets;
}

void PlayerPetsDAO::updatePetName(model::gameobjects::player::PetCommonData& petCommonData) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement("UPDATE player_pets SET name = ? WHERE id = ?");
		stmt->setString(1, petCommonData.getName());
		stmt->setInt(2, petCommonData.getObjectId());
		stmt->execute();
	} catch (const std::exception& e) {
		log.error("Error update pet #" + std::to_string(petCommonData.getObjectId()), e);
	}
}

bool PlayerPetsDAO::savePetMoodData(model::gameobjects::player::PetCommonData& petCommonData) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(
			"UPDATE player_pets SET mood_started = ?, counter = ?, mood_cd_started = ?, gift_cd_started = ?, despawn_time = ? WHERE id = ?");
		stmt->setLong(1, petCommonData.getMoodStartTime());
		stmt->setInt(2, petCommonData.getShuggleCounter());
		stmt->setLong(3, petCommonData.getMoodCdStarted());
		stmt->setLong(4, petCommonData.getGiftCdStarted());
		stmt->setTimestamp(5, petCommonData.getDespawnTime());
		stmt->setInt(6, petCommonData.getObjectId());
		stmt->execute();
	} catch (const std::exception& e) {
		log.error("Error updating mood for pet #" + std::to_string(petCommonData.getObjectId()), e);
		return false;
	}
	return true;
}

std::vector<int32_t> PlayerPetsDAO::getUsedIDs() {
	return detail::getUsedIDs(log, "SELECT id FROM player_pets", "id", "Can't get list of IDs from pets table");
}

} // namespace aion::gameserver::dao
