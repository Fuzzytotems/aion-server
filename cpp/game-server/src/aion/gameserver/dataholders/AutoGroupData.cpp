#include "aion/gameserver/dataholders/AutoGroupData.h"

#include <algorithm>

#include "aion/gameserver/dataholders/detail/JavaHashMapOrder.h"

namespace aion::gameserver::dataholders {

void AutoGroupData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	detail::JavaHashMapOrder<int32_t, bool> portalNpcOrder;
	for (const model::autogroup::AutoGroup& ag : autoGroup) {
		autoGroupByInstanceId.insert_or_assign(ag.getMaskId(), &ag);
		if (ag.isRecruitableInstance()) {
			for (int32_t npcId : ag.getNpcIds()) {
				// Java: recruitableInstanceMaskIdByPortalNpc.computeIfAbsent(npcId, k -> new ArrayList<>()).add(ag.getMaskId())
				recruitableInstanceMaskIdByPortalNpc[npcId].push_back(ag.getMaskId());
				portalNpcOrder.computeIfAbsent(npcId, true, detail::javaHashCode(npcId));
			}
		}
	}
	portalNpcsInHashOrder = portalNpcOrder.keys();
	// Java: autoGroup = null (the C++ index points into the storage, which stays)
}

const model::autogroup::AutoGroup* AutoGroupData::getTemplateByInstanceMaskId(int32_t maskId) const {
	auto it = autoGroupByInstanceId.find(maskId);
	return it != autoGroupByInstanceId.end() ? it->second : nullptr;
}

const std::vector<int32_t>* AutoGroupData::getRecruitableInstanceMaskIds(int32_t portalNpcId) const {
	auto it = recruitableInstanceMaskIdByPortalNpc.find(portalNpcId);
	return it != recruitableInstanceMaskIdByPortalNpc.end() ? &it->second : nullptr;
}

std::vector<int32_t> AutoGroupData::getRecruitableInstanceMaskIds() const {
	// Java: values().stream().flatMap(Collection::stream).distinct().toList()
	std::vector<int32_t> result;
	for (int32_t npcId : portalNpcsInHashOrder) {
		for (int32_t maskId : recruitableInstanceMaskIdByPortalNpc.at(npcId)) {
			if (std::find(result.begin(), result.end(), maskId) == result.end())
				result.push_back(maskId);
		}
	}
	return result;
}

int32_t AutoGroupData::size() const {
	return static_cast<int32_t>(autoGroupByInstanceId.size());
}

} // namespace aion::gameserver::dataholders
