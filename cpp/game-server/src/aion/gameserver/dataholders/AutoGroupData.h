#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "aion/gameserver/dataholders/AutoGroupData.xml.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.AutoGroupData.
 * <p>
 * C++: the index points into the bound `autoGroup` storage, which stays after afterUnmarshal (static-data.md §2.6). The portal npc index is
 * built from AutoGroup::getNpcIds and isRecruitableInstance (header request holders-1).
 */
class AutoGroupData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/AutoGroupData.xml.inc"
private:
	std::unordered_map<int32_t, const model::autogroup::AutoGroup*> autoGroupByInstanceId;
	std::unordered_map<int32_t, std::vector<int32_t>> recruitableInstanceMaskIdByPortalNpc;
	/** C++ only: the portal npc ids of recruitableInstanceMaskIdByPortalNpc in Java's HashMap iteration order */
	std::vector<int32_t> portalNpcsInHashOrder;

public:
	/** @return the auto group, nullptr (Java null) if there is none */
	const model::autogroup::AutoGroup* getTemplateByInstanceMaskId(int32_t maskId) const;

	/** @return the mask ids of the recruitable instances of the portal npc, nullptr (Java null) if there are none */
	const std::vector<int32_t>* getRecruitableInstanceMaskIds(int32_t portalNpcId) const;

	/** @return all recruitable instance mask ids, distinct, in Java's order (the values of the HashMap in iteration order) */
	std::vector<int32_t> getRecruitableInstanceMaskIds() const;

	int32_t size() const;
};

} // namespace aion::gameserver::dataholders
