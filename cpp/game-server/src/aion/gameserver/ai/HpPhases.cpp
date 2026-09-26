#include "aion/gameserver/ai/HpPhases.h"

#include <algorithm>
#include <string>
#include <typeinfo>
#include <vector>

#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/ai/NpcAI.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/sync/Monitor.h"

namespace aion::gameserver::ai {

namespace {

/** Java: the checkcast javac inserts for `T extends NpcAI & PhaseHandler` where the AI is used as a PhaseHandler */
HpPhases::PhaseHandler& asPhaseHandler(NpcAI& ai) {
	auto* handler = dynamic_cast<HpPhases::PhaseHandler*>(&ai);
	if (handler == nullptr)
		throw runtime::ClassCastException(
			std::string(typeid(ai).name()) + " cannot be cast to class com.aionemu.gameserver.ai.HpPhases$PhaseHandler");
	return *handler;
}

} // namespace

HpPhases::HpPhases(int32_t hpPercent, std::initializer_list<int32_t> moreHpPercents) {
	// Java: IntStream.concat(IntStream.of(hpPercent), IntStream.of(moreHpPercents)).distinct().forEach(phaseHpPercents::add)
	std::vector<int32_t> distinct;
	distinct.push_back(hpPercent);
	for (int32_t percent : moreHpPercents) {
		if (std::find(distinct.begin(), distinct.end(), percent) == distinct.end())
			distinct.push_back(percent);
	}
	for (int32_t percent : distinct)
		phaseHpPercents.add(percent);
	phaseHpPercents.sort([](int32_t a, int32_t b) { return a > b; }); // sort percents in descending order
}

HpPhases::~HpPhases() = default;

runtime::Ref<HpPhases> HpPhases::create(int32_t hpPercent, std::initializer_list<int32_t> moreHpPercents) {
	return runtime::makeRef<HpPhases>(hpPercent, moreHpPercents);
}

void HpPhases::reset() {
	SYNCHRONIZED(phaseHpPercents) {
		currentPhase.set(0);
	}
}

void HpPhases::tryEnterNextPhase(NpcAI& ai) {
	if (!ai.getOwner().isSpawned() || ai.isDead() || ai.isInState(AIState::RETURNING))
		return;
	SYNCHRONIZED(phaseHpPercents) {
		if (currentPhase.get() >= phaseHpPercents.size())
			return;
		int32_t phaseHpPercent = phaseHpPercents.get(currentPhase.get());
		// Java: ai.getLifeStats() (NpcAI.java:51-53), which is `return getOwner().getLifeStats();` - NpcAI's narrowing accessor is Java-protected
		// (package access inside com.aionemu.gameserver.ai), so the C++ port spells the delegation out; same call, same object.
		if (phaseHpPercent >= ai.getOwner().getLifeStats()->getHpPercentage()) {
			currentPhase.set(currentPhase.get() + 1);
			asPhaseHandler(ai).handleHpPhase(phaseHpPercent);
		}
	}
}

} // namespace aion::gameserver::ai
