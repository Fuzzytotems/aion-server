#include "aion/gameserver/dao/SurveyControllerDAO.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::dao {

namespace {

// Java SQL constants of the class, used only by the bodies (P4-14 ports them with the DAO methods).
constexpr std::string_view UPDATE_QUERY = "UPDATE `surveys` SET `used`=?, used_time=NOW() WHERE `unique_id`=?";
constexpr std::string_view SELECT_QUERY = "SELECT * FROM `surveys` WHERE `used`=?";

} // namespace

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.SurveyControllerDAO");

std::vector<runtime::Ref<model::templates::survey::SurveyItem>> SurveyControllerDAO::getAllUnused() {
	AION_UNPORTED();
}

bool SurveyControllerDAO::useItem(int32_t id) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::dao
