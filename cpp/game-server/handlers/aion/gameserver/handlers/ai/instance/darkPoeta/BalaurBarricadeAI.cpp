#include "aion/gameserver/handlers/ai/instance/darkPoeta/BalaurBarricadeAI.h"

namespace aion::gameserver::handlers::ai::instance::darkPoeta {

AION_AI(BalaurBarricadeAI, "balaurbarricade");

// Java BalaurBarricadeAI.java:22-26
void BalaurBarricadeAI::handleAttack(runtime::Ptr<Creature> creature) {
	OneDmgNoActionAI::handleAttack(creature);
	hpPhases->tryEnterNextPhase(*this);
}

// Java BalaurBarricadeAI.java:28-34
void BalaurBarricadeAI::handleHpPhase(int32_t phaseHpPercent) {
	switch (phaseHpPercent) {
		case 50:
			spawnProtectors(true);
			break;
		case 10:
			spawnProtectors(false);
			break;
	}
}

// Java BalaurBarricadeAI.java:36-51
void BalaurBarricadeAI::spawnProtectors(bool isFirstSpawn) {
	switch (getNpcId()) {
		case 700517:
			spawn(isFirstSpawn ? 215262 : 214883, 282.2922f, 1003.0374f, 113.1999f, int8_t{25});
			spawn(isFirstSpawn ? 215262 : 215263, 289.5031f, 1000.1637f, 112.9796f, int8_t{25});
			break;
		case 700556:
			spawn(isFirstSpawn ? 215262 : 214883, 315.8379f, 982.8948f, 111.0691f, int8_t{17});
			spawn(isFirstSpawn ? 215262 : 215263, 309.0993f, 989.5142f, 112.6760f, int8_t{17});
			break;
		case 700558:
			spawn(isFirstSpawn ? 215262 : 214883, 199.7505f, 843.6876f, 100.6562f, int8_t{59});
			spawn(isFirstSpawn ? 215262 : 215263, 201.9819f, 853.4918f, 101.0603f, int8_t{59});
			break;
	}
}

// Java BalaurBarricadeAI.java:53-57
void BalaurBarricadeAI::handleBackHome() {
	OneDmgNoActionAI::handleBackHome();
	hpPhases->reset();
}

} // namespace aion::gameserver::handlers::ai::instance::darkPoeta
