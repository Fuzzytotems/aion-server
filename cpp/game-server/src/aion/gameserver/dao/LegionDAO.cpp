#include "aion/gameserver/dao/LegionDAO.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::dao {

// Anonymous classes and stored lambdas of the Java class (hub-headers.md §7.3): the bodies that create them define the structs that
// `python tools/gen/fieldmap.py --class <key>` prints.
//   anonymous IUStH at LegionDAO.java:64 (com.aionemu.gameserver.dao.LegionDAO$1); argument 2 of insertUpdate(); storage: sync
//   anonymous IUStH at LegionDAO.java:77 (com.aionemu.gameserver.dao.LegionDAO$2); argument 2 of insertUpdate(); storage: sync
//   anonymous ParamReadStH at LegionDAO.java:101 (com.aionemu.gameserver.dao.LegionDAO$3); argument 2 of select(); storage: sync
//   anonymous ParamReadStH at LegionDAO.java:132 (com.aionemu.gameserver.dao.LegionDAO$4); argument 2 of select(); storage: sync
//   anonymous IUStH at LegionDAO.java:260 (com.aionemu.gameserver.dao.LegionDAO$5); argument 2 of insertUpdate(); storage: sync
//   anonymous IUStH at LegionDAO.java:278 (com.aionemu.gameserver.dao.LegionDAO$6); argument 2 of insertUpdate(); storage: sync
//   anonymous ParamReadStH at LegionDAO.java:298 (com.aionemu.gameserver.dao.LegionDAO$7); argument 2 of select(); storage: sync

namespace {

// Java SQL constants of the class, used only by the bodies (P4-14 ports them with the DAO methods).
constexpr std::string_view INSERT_LEGION_QUERY = "INSERT INTO legions(id, `name`) VALUES (?, ?)";
constexpr std::string_view SELECT_LEGION_QUERY1 = "SELECT * FROM legions WHERE id=?";
constexpr std::string_view SELECT_LEGION_QUERY2 = "SELECT * FROM legions WHERE name=?";
constexpr std::string_view DELETE_LEGION_QUERY = "DELETE FROM legions WHERE id = ?";
constexpr std::string_view UPDATE_LEGION_QUERY = "UPDATE legions SET name=?, level=?, contribution_points=?, deputy_permission=?, centurion_permission=?, legionary_permission=?, volunteer_permission=?, disband_time=?, occupied_legion_dominion=?, last_legion_dominion=?, current_legion_dominion=? WHERE id=?";
constexpr std::string_view INSERT_ANNOUNCEMENT_QUERY = "INSERT INTO legion_announcement_list(`legion_id`, `announcement`, `date`) VALUES (?, ?, ?)";
constexpr std::string_view SELECT_ANNOUNCEMENT_QUERY = "SELECT * FROM legion_announcement_list WHERE legion_id = ? ORDER BY date DESC LIMIT 1";
constexpr std::string_view DELETE_ANNOUNCEMENT_QUERY = "DELETE FROM legion_announcement_list WHERE legion_id = ?";
constexpr std::string_view INSERT_EMBLEM_QUERY = "INSERT INTO legion_emblems(legion_id, emblem_id, color_a, color_r, color_g, color_b, emblem_type, emblem_data) VALUES (?, ?, ?, ?, ?, ?, ?, ?)";
constexpr std::string_view UPDATE_EMBLEM_QUERY = "UPDATE legion_emblems SET emblem_id=?, color_a=?, color_r=?, color_g=?, color_b=?, emblem_type=?, emblem_data=? WHERE legion_id=?";
constexpr std::string_view SELECT_EMBLEM_QUERY = "SELECT * FROM legion_emblems WHERE legion_id=?";
constexpr std::string_view INSERT_HISTORY_QUERY = "INSERT INTO legion_history(`legion_id`, `date`, `history_type`, `name`, `description`) VALUES (?, ?, ?, ?, ?)";
constexpr std::string_view SELECT_HISTORY_QUERY = "SELECT * FROM `legion_history` WHERE legion_id=? ORDER BY date DESC, id DESC";
constexpr std::string_view DELETE_HISTORY_QUERY = "DELETE FROM `legion_history` WHERE id IN (%s)";

} // namespace

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.LegionDAO");

bool LegionDAO::isNameUsed(std::string_view name) {
	AION_UNPORTED();
}

bool LegionDAO::saveNewLegion(model::team::legion::Legion& legion) {
	AION_UNPORTED();
}

void LegionDAO::storeLegion(model::team::legion::Legion& legion) {
	AION_UNPORTED();
}

runtime::Ref<model::team::legion::Legion> LegionDAO::loadLegion(std::string_view legionName) {
	AION_UNPORTED();
}

runtime::Ref<model::team::legion::Legion> LegionDAO::loadLegion(int32_t legionId) {
	AION_UNPORTED();
}

void LegionDAO::deleteLegion(int32_t legionId) {
	AION_UNPORTED();
}

std::vector<int32_t> LegionDAO::getUsedIDs() {
	AION_UNPORTED();
}

runtime::Ref<model::team::legion::Legion::Announcement> LegionDAO::loadAnnouncement(int32_t legionId) {
	AION_UNPORTED();
}

void LegionDAO::saveAnnouncement(int32_t legionId, runtime::Ptr<model::team::legion::Legion::Announcement> announcement) {
	AION_UNPORTED();
}

void LegionDAO::storeLegionEmblem(int32_t legionId, model::team::legion::LegionEmblem& legionEmblem) {
	AION_UNPORTED();
}

bool LegionDAO::validEmblem(model::team::legion::LegionEmblem& legionEmblem) {
	AION_UNPORTED();
}

bool LegionDAO::checkEmblem(int32_t legionid) {
	AION_UNPORTED();
}

void LegionDAO::createLegionEmblem(int32_t legionId, model::team::legion::LegionEmblem& legionEmblem) {
	AION_UNPORTED();
}

void LegionDAO::updateLegionEmblem(int32_t legionId, model::team::legion::LegionEmblem& legionEmblem) {
	AION_UNPORTED();
}

runtime::Ref<model::team::legion::LegionEmblem> LegionDAO::loadLegionEmblem(int32_t legionId) {
	AION_UNPORTED();
}

void LegionDAO::loadHistory(model::team::legion::Legion& legion) {
	AION_UNPORTED();
}

runtime::Ref<model::team::legion::LegionHistoryEntry> LegionDAO::insertHistory(int32_t legionId, model::team::legion::LegionHistoryAction action,
	std::string_view name, std::string_view description) {
	AION_UNPORTED();
}

void LegionDAO::deleteHistory(int32_t legionId, const std::vector<runtime::Ptr<model::team::legion::LegionHistoryEntry>>& entries) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::dao
