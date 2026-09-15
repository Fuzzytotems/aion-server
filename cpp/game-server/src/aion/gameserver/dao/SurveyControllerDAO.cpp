#include "aion/gameserver/dao/SurveyControllerDAO.h"

#include <string>
#include <string_view>

#include "aion/commons/database/DatabaseFactory.h"
#include "aion/commons/database/PreparedStatement.h"
#include "aion/commons/database/ResultSet.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/model/templates/survey/SurveyItem.h"

namespace aion::gameserver::dao {

using commons::database::DatabaseFactory;

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.SurveyControllerDAO");

namespace {
constexpr std::string_view UPDATE_QUERY = "UPDATE `surveys` SET `used`=?, used_time=NOW() WHERE `unique_id`=?";
constexpr std::string_view SELECT_QUERY = "SELECT * FROM `surveys` WHERE `used`=?";
} // namespace

std::vector<runtime::Ref<model::templates::survey::SurveyItem>> SurveyControllerDAO::getAllUnused() {
	std::vector<runtime::Ref<model::templates::survey::SurveyItem>> list;
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(SELECT_QUERY);
		stmt->setInt(1, 0);
		auto rset = stmt->executeQuery();
		while (rset->next()) {
			runtime::Ref<model::templates::survey::SurveyItem> item = model::templates::survey::SurveyItem::create();
			item->uniqueId = rset->getInt("unique_id");
			item->ownerId = rset->getInt("owner_id");
			item->itemId = rset->getInt("item_id");
			item->count = rset->getLong("item_count");
			item->html = rset->getString("html_text");
			item->radio = rset->getString("html_radio");
			list.push_back(std::move(item));
		}
	} catch (const std::exception& e) {
		log.error("Could not load new surveys", e);
	}
	return list;
}

bool SurveyControllerDAO::useItem(int32_t id) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(UPDATE_QUERY);
		stmt->setInt(1, 1);
		stmt->setInt(2, id);
		stmt->execute();
	} catch (const std::exception& e) {
		log.error("Could not set used state for survey " + std::to_string(id), e);
		return false;
	}
	return true;
}

} // namespace aion::gameserver::dao
