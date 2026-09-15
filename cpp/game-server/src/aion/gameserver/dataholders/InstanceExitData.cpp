#include "aion/gameserver/dataholders/InstanceExitData.h"

#include "aion/gameserver/model/Race.h"
namespace aion::gameserver::dataholders {

using model::templates::portal::InstanceExit;

void InstanceExitData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	for (const InstanceExit& exit : instanceExit)
		instanceExitByWorldId[exit.getInstanceId()].push_back(&exit);
	// Java: instanceExit = null (the C++ lists point into the storage, which stays)
}

const InstanceExit* InstanceExitData::getInstanceExit(int32_t worldId, model::Race race) const {
	auto it = instanceExitByWorldId.find(worldId);
	if (it == instanceExitByWorldId.end() || it->second.empty())
		return nullptr;
	for (const InstanceExit* exit : it->second) {
		if (exit->getRace() == model::Race::PC_ALL || exit->getRace() == race)
			return exit;
	}
	return nullptr;
}

int32_t InstanceExitData::size() const {
	int32_t sum = 0;
	for (const auto& [worldId, exits] : instanceExitByWorldId)
		sum += static_cast<int32_t>(exits.size());
	return sum;
}

} // namespace aion::gameserver::dataholders
