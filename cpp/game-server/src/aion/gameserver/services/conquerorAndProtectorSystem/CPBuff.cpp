#include "aion/gameserver/services/conquerorAndProtectorSystem/CPBuff.h"

#include <memory>
#include <vector>

#include "aion/gameserver/dataholders/ConquerorAndProtectorData.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/stats/calc/functions/IStatFunction.h"
#include "aion/gameserver/model/stats/calc/functions/StatFunction.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/model/templates/cp/CPRank.h"
#include "aion/gameserver/model/templates/cp/CPType.h"

namespace aion::gameserver::services::conquerorAndProtectorSystem {

CPBuff::CPBuff() {
}

runtime::Ref<CPBuff> CPBuff::create() {
	return runtime::makeRef<CPBuff>();
}

// Java CPBuff.java:14-23
void CPBuff::applyEffect(model::gameobjects::player::Player& player, model::templates::cp::CPType type, int32_t rank) {
	endEffect(player);

	if (rank == 0)
		return;

	const model::templates::cp::CPRank* cpRank = dataholders::DataManager::CONQUEROR_AND_PROTECTOR_DATA->getRank(type, rank);
	if (cpRank != nullptr && !cpRank->getStatModifiers().empty()) {
		std::vector<runtime::Ptr<model::stats::calc::functions::IStatFunction>> functions;
		for (const std::unique_ptr<model::stats::calc::functions::StatFunction>& modifier : cpRank->getStatModifiers())
			functions.emplace_back(model::stats::calc::functions::StatFunction::ofTemplate(modifier.get()));
		player.getGameStats()->addEffect(runtime::Ptr<model::stats::calc::StatOwner>(static_cast<model::stats::calc::StatOwner&>(*this)), functions);
	}
}

// Java CPBuff.java:25-27
void CPBuff::endEffect(model::gameobjects::player::Player& player) {
	player.getGameStats()->endEffect(*this);
}

CPBuff::~CPBuff() = default;

} // namespace aion::gameserver::services::conquerorAndProtectorSystem
