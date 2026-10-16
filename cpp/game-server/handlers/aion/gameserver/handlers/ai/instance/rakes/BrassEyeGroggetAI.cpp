#include "aion/gameserver/handlers/ai/instance/rakes/BrassEyeGroggetAI.h"

#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/model/templates/ai/Percentage.h"

namespace aion::gameserver::handlers::ai::instance::rakes {

AION_AI(BrassEyeGroggetAI, "brasseyegrogget");

// todo 4 towers in the room center and fix coordinates of monsters
// need snif

// Java BrassEyeGroggetAI.java:22-25
void BrassEyeGroggetAI::handleSpawned() {
	SummonerAI::handleSpawned();
}

// Java BrassEyeGroggetAI.java:27-30
void BrassEyeGroggetAI::handleIndividualSpawnedSummons(const model::templates::ai::Percentage& percent) {
	spawn(percent.getPercent());
}

// Java BrassEyeGroggetAI.java:32-53 (the anonymous Runnable: a task pinning this)
void BrassEyeGroggetAI::spawn(int32_t percent) {
	int32_t i = 0;
	if (percent < 81 && percent > 60) {
		i = 1;
	} else if (percent < 61 && percent > 30) {
		i = 2;
	} else if (percent < 31) {
		i = 3;
	}
	const int32_t nrSpawn = i;

	// to do move boss to initial position and set pause move and atack
	// after 9 sec first spawn
	ThreadPoolManager::getInstance().schedule({this}, [this, nrSpawn] { spawnHelpers1(nrSpawn); }, 9000);
}

// Java BrassEyeGroggetAI.java:55-79
void BrassEyeGroggetAI::spawnHelpers1(int32_t nrSpawn) {
	switch (nrSpawn) {
		case 1:
			spawn(281184, 381.3756f, 495.24835f, 1072.1212f, int8_t{13});
			spawn(281181, 379.4199f, 495.36453f, 1072.1212f, int8_t{13});
			break;
		case 2:
			spawn(281184, 383.76724f, 527.02856f, 1072.1212f, int8_t{100});
			spawn(281181, 381.26767f, 526.40845f, 1072.1212f, int8_t{100});
			break;
		case 3:
			spawn(281182, 416.2482f, 500.6516f, 1071.8457f, int8_t{52});
			spawn(281182, 415.66647f, 519.5354f, 1071.8457f, int8_t{52});
			break;
		default:
			break;
	}

	// next spawn after 35 sec
	ThreadPoolManager::getInstance().schedule({this}, [this, nrSpawn] { spawnHelpers2(nrSpawn); }, 35000);
}

// Java BrassEyeGroggetAI.java:81-111
void BrassEyeGroggetAI::spawnHelpers2(int32_t nrSpawn) {
	switch (nrSpawn) {
		case 1:
			spawn(281183, 383.26193f, 528.38403f, 1072.1212f, int8_t{100});
			spawn(281184, 383.76724f, 527.02856f, 1072.1212f, int8_t{100});
			spawn(281181, 381.26767f, 526.40845f, 1072.1212f, int8_t{100});
			break;
		case 2:
			spawn(281182, 429.55338f, 525.7714f, 1075.3801f, int8_t{62});
			spawn(281182, 429.52865f, 492.56076f, 1075.3801f, int8_t{62});
			spawn(281181, 376.42566f, 502.19736f, 1072.1212f, int8_t{1});
			break;
		case 3:
			spawn(281187, 376.42566f, 502.19736f, 1072.1212f, int8_t{1});
			spawn(281181, 381.26767f, 526.40845f, 1072.1212f, int8_t{100});
			break;
		default:
			break;
	}

	// remove effect after 21 sec
	ThreadPoolManager::getInstance().schedule({this}, [this] {
		if (getEffectController()->hasAbnormalEffect(18191)) {
			getEffectController()->removeEffect(18191);
		}
		// to do move boss in the room center and remove pause
		// to do some skill boss use
	}, 21000);
}

} // namespace aion::gameserver::handlers::ai::instance::rakes
