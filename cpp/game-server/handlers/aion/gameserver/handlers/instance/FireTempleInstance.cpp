#include "aion/gameserver/handlers/instance/FireTempleInstance.h"

#include "aion/commons/utils/Rnd.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/main/RuntimeConfig.h"
#include "aion/gameserver/custom/difficulty/DifficultyMenu.h"
#include "aion/gameserver/model/ChatType.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/calc/functions/StatFunction.h"
#include "aion/gameserver/model/stats/calc/functions/StatSetFunction.h"
#include "aion/gameserver/model/stats/container/NpcGameStats.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"

namespace aion::gameserver::handlers::instance {

AION_INSTANCE_HANDLER(FireTempleInstance, 320100000);

// Java FireTempleInstance.java:14-16
FireTempleInstance::FireTempleInstance(WorldMapInstance& instance)
	: GeneralInstanceHandler(instance), difficulty(custom::difficulty::DifficultyMenu::difficultyOfNewInstance(320100000)) {
	if (difficulty == custom::difficulty::DifficultyMenu::HARD && configs::main::RuntimeConfig::INSTANCE_SCORING_ENABLE.load()) {
		runtime::Ref<custom::instance::InstanceScoring> score =
			custom::instance::InstanceScoring::create(scoringSettings(), [] { return commons::utils::currentTimeMillis(); });
		score->setTitle("Fire Temple - Hard");
		scoring.set(score);
	}
}

runtime::Ref<FireTempleInstance> FireTempleInstance::create(WorldMapInstance& instance) {
	return runtime::makeRef<FireTempleInstance>(instance);
}

// Java FireTempleInstance.java:19-62
void FireTempleInstance::onInstanceCreate() {
	// Random spawns of bosses
	if (commons::utils::Rnd::chance() < 75) { // Blue Crystal Molgat
		spawn(212839, 127.1218f, 176.1912f, 99.67548f, int8_t{15});
	} else { // elite mob spawns
		spawn(212790, 127.1218f, 176.1912f, 99.67548f, int8_t{15});
	}
	if (commons::utils::Rnd::chance() < 75) { // Black Smoke Asparn
		spawn(212842, 322.3193f, 431.2696f, 134.5296f, int8_t{80});
	} else { // elite mob spawns
		spawn(212799, 322.3193f, 431.2696f, 134.5296f, int8_t{80});
	}
	if (commons::utils::Rnd::chance() < 75) { // Lava Gatneri
		spawn(212840, 153.0038f, 299.7786f, 123.0186f, int8_t{30});
	} else { // elite mob spawns
		spawn(212794, 153.0038f, 299.7786f, 123.0186f, int8_t{30});
	}
	if (commons::utils::Rnd::chance() < 75) { // Tough Sipus
		spawn(212843, 296.6911f, 201.9092f, 119.3652f, int8_t{30});
	} else { // elite mob spawns
		spawn(212803, 296.6911f, 201.9092f, 119.3652f, int8_t{15});
	}
	if (commons::utils::Rnd::chance() < 75) { // Flame Branch Flavi
		spawn(212841, 350.9276f, 351.7389f, 146.8498f, int8_t{45});
	} else { // elite mob spawns
		spawn(212799, 350.9276f, 351.7389f, 146.8498f, int8_t{45});
	}
	if (commons::utils::Rnd::chance() < 75) { // Broken Wing Kutisen
		spawn(212845, 298.7095f, 89.42245f, 128.7143f, int8_t{15});
	} else { // elite mob spawns
		spawn(214094, 298.7095f, 89.42245f, 128.7143f, int8_t{15});
	}
	if (commons::utils::Rnd::chance() < 10) { // stronger kromede
		spawn(214621, 421.9935f, 93.18915f, 117.3053f, int8_t{46});
	} else { // normal kromede
		spawn(212846, 421.9935f, 93.18915f, 117.3053f, int8_t{46});
	}
}

// C++-dev only (the difficulty menu example)
void FireTempleInstance::onSpawn(VisibleObject& object) {
	if (difficulty != custom::difficulty::DifficultyMenu::HARD)
		return;
	if (runtime::Ptr<Npc> npc = runtime::as<Npc>(runtime::Ptr<VisibleObject>(&object)))
		applyHardMode(*npc);
}

// C++-dev only (the difficulty menu example)
void FireTempleInstance::onEnterInstance(Player& player) {
	if (difficulty == custom::difficulty::DifficultyMenu::HARD)
		PacketSendUtility::sendMessage(player, HARD_MODE_MESSAGE, model::ChatType::BRIGHT_YELLOW_CENTER);
	runtime::Ptr<custom::instance::InstanceScoring> score = scoring.get();
	if (score != nullptr && score->start(*instance)) // the countdown starts at the first entry
		scoringTimeout.set(ThreadPoolManager::getInstance().schedule({this}, [this] { onScoringTimeout(); },
			int64_t{score->getSettings().timeLimitSeconds} * 1000));
}

// C++-dev only (instance scoring)
void FireTempleInstance::onScoringTimeout() {
	if (runtime::Ptr<custom::instance::InstanceScoring> score = scoring.get())
		score->finish(*instance, false);
}

// C++-dev only (instance scoring); Java's FireTempleInstance has no onDie
void FireTempleInstance::onDie(Npc& npc) {
	GeneralInstanceHandler::onDie(npc);
	runtime::Ptr<custom::instance::InstanceScoring> score = scoring.get();
	if (score == nullptr)
		return;
	score->onKill(*instance, npc);
	if (score->isFinished())
		if (runtime::Ptr<runtime::Future> task = scoringTimeout.get())
			task->cancel(false);
}

// C++-dev only (instance scoring)
void FireTempleInstance::onInstanceDestroy() {
	GeneralInstanceHandler::onInstanceDestroy();
	if (runtime::Ptr<runtime::Future> task = scoringTimeout.get())
		task->cancel(false);
}

// C++-dev only: 20 minutes; the seven named bosses (onInstanceCreate's spawns) count as bosses; Kromede, either form, ends the run
custom::instance::InstanceScoring::Settings FireTempleInstance::scoringSettings() {
	custom::instance::InstanceScoring::Settings settings;
	settings.timeLimitSeconds = 20 * 60;
	settings.bossNpcIds = {212839, 212842, 212840, 212843, 212841, 212845, 212846, 214621};
	settings.finalBossNpcIds = {212846, 214621};
	// 162000049 Greater Recovery Serum, 162000005 Greater Life Potion, 162000010 Greater Mana Potion (level 40 consumables)
	settings.rewards = {{
		{{162000049, 10}, {162000005, 20}}, // rank 1
		{{162000049, 5}, {162000005, 15}},  // rank 2
		{{162000005, 10}, {162000010, 10}}, // rank 3
		{{162000005, 5}},                   // rank 4
		{{162000005, 2}},                   // rank 5
		{},                                 // 6 and 7: not reached by this shape
		{},
		{}, // rank 8: the time out
	}};
	return settings;
}

// C++-dev only (the difficulty menu example)
void FireTempleInstance::applyHardMode(Npc& npc) {
	using model::stats::calc::functions::RcStatFunction;
	using model::stats::calc::functions::StatSetFunction;
	using model::stats::container::StatEnum;
	const int32_t maxHp = npc.getLifeStats()->getMaxHp();
	const int32_t physicalAttack = npc.getGameStats()->getMainHandPAttack({})->getCurrent();
	const int32_t magicalAttack = npc.getGameStats()->getMainHandMAttack({})->getCurrent();
	std::vector<runtime::Ref<model::stats::calc::functions::IStatFunction>> functions{
		RcStatFunction<StatSetFunction>::create(StatEnum::MAXHP, maxHp * 2),
		RcStatFunction<StatSetFunction>::create(StatEnum::PHYSICAL_ATTACK, physicalAttack * 3 / 2),
		RcStatFunction<StatSetFunction>::create(StatEnum::MAGICAL_ATTACK, magicalAttack * 3 / 2)};
	std::vector<runtime::Ptr<model::stats::calc::functions::IStatFunction>> borrowed(functions.begin(), functions.end());
	npc.getGameStats()->addEffect(nullptr, borrowed);
	npc.getLifeStats()->setCurrentHp(npc.getLifeStats()->getMaxHp());
}

} // namespace aion::gameserver::handlers::instance
