#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "aion/gameserver/dao/fwd.h"
#include "aion/gameserver/model/team/legion/Legion.h"
#include "aion/gameserver/model/team/legion/fwd.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::dao {

/**
 * Class that is responsible for storing/loading legion data
 *
 * @author Simple, cura
 */
class LegionDAO {
public:
	static bool isNameUsed(std::string_view name);
	static bool saveNewLegion(model::team::legion::Legion& legion);
	static void storeLegion(model::team::legion::Legion& legion);
	static runtime::Ref<model::team::legion::Legion> loadLegion(std::string_view legionName);
	static runtime::Ref<model::team::legion::Legion> loadLegion(int32_t legionId);
	static void deleteLegion(int32_t legionId);
	static std::vector<int32_t> getUsedIDs();
	static runtime::Ref<model::team::legion::Legion::Announcement> loadAnnouncement(int32_t legionId);
	static void saveAnnouncement(int32_t legionId, runtime::Ptr<model::team::legion::Legion::Announcement> announcement);
	static void storeLegionEmblem(int32_t legionId, model::team::legion::LegionEmblem& legionEmblem);
private:
	static bool validEmblem(model::team::legion::LegionEmblem& legionEmblem);
public:
	static bool checkEmblem(int32_t legionid);
private:
	static void createLegionEmblem(int32_t legionId, model::team::legion::LegionEmblem& legionEmblem);
	static void updateLegionEmblem(int32_t legionId, model::team::legion::LegionEmblem& legionEmblem);
public:
	static runtime::Ref<model::team::legion::LegionEmblem> loadLegionEmblem(int32_t legionId);
	static void loadHistory(model::team::legion::Legion& legion);
	static runtime::Ref<model::team::legion::LegionHistoryEntry> insertHistory(int32_t legionId, model::team::legion::LegionHistoryAction action,
		std::string_view name, std::string_view description);
	static void deleteHistory(int32_t legionId, const std::vector<runtime::Ptr<model::team::legion::LegionHistoryEntry>>& entries);
};

} // namespace aion::gameserver::dao
