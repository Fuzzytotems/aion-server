#include "aion/gameserver/handlers/ai/instance/rakes/ChiefGunnerKoakoaAI.h"

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/model/templates/ai/Percentage.h"

namespace aion::gameserver::handlers::ai::instance::rakes {

AION_AI(ChiefGunnerKoakoaAI, "gunnerkoakoa");

// Java ChiefGunnerKoakoaAI.java:21-27
void ChiefGunnerKoakoaAI::handleIndividualSpawnedSummons(const model::templates::ai::Percentage& /*percent*/) {
	if (getEffectController()->hasAbnormalEffect(18552)) {
		checkAbnormalEffect();
	}
	randomSpawn(commons::utils::Rnd::get(1, 3));
}

// Java ChiefGunnerKoakoaAI.java:29-38 (the anonymous Runnable: a task pinning this)
void ChiefGunnerKoakoaAI::checkAbnormalEffect() {
	ThreadPoolManager::getInstance().schedule({this}, [this] {
		getEffectController()->removeEffect(18552);
		// to do remove pause
	}, 21000);
}

// Java ChiefGunnerKoakoaAI.java:40-93
void ChiefGunnerKoakoaAI::randomSpawn(int32_t i) {
	// to do pause boss
	spawn(281212, 757.39746f, 508.70383f, 1012.30084f, int8_t{0});
	switch (i) {
		case 1:
			spawn(281212, 726.1167f, 503.28836f, 1012.6846f, int8_t{0});
			spawn(281212, 736.4446f, 505.3141f, 1012.1576f, int8_t{0});
			spawn(281212, 746.9261f, 503.50122f, 1012.68335f, int8_t{0});
			spawn(281212, 728.9705f, 492.59402f, 1012.68335f, int8_t{0});
			spawn(281212, 739.9526f, 491.54123f, 1011.692f, int8_t{0});
			spawn(281212, 749.754f, 491.74677f, 1011.8663f, int8_t{0});
			spawn(281212, 756.9996f, 500.01736f, 1011.692f, int8_t{0});
			spawn(281213, 736.9722f, 514.6446f, 1011.8599f, int8_t{0});
			spawn(281213, 747.5162f, 514.51715f, 1011.692f, int8_t{0});
			spawn(281213, 726.8303f, 514.5155f, 1012.6845f, int8_t{0});
			spawn(281213, 727.9019f, 524.578f, 1012.68365f, int8_t{0});
			spawn(281213, 738.52844f, 525.0482f, 1011.692f, int8_t{0});
			spawn(281213, 758.3127f, 520.59143f, 1011.692f, int8_t{0});
			spawn(281213, 748.7474f, 525.84f, 1011.859f, int8_t{0});
			break;
		case 2:
			spawn(281213, 726.1167f, 503.28836f, 1012.6846f, int8_t{0});
			spawn(281213, 736.4446f, 505.3141f, 1012.1576f, int8_t{0});
			spawn(281212, 746.9261f, 503.50122f, 1012.68335f, int8_t{0});
			spawn(281213, 728.9705f, 492.59402f, 1012.68335f, int8_t{0});
			spawn(281213, 739.9526f, 491.54123f, 1011.692f, int8_t{0});
			spawn(281212, 749.754f, 491.74677f, 1011.8663f, int8_t{0});
			spawn(281212, 756.9996f, 500.01736f, 1011.692f, int8_t{0});
			spawn(281212, 736.9722f, 514.6446f, 1011.8599f, int8_t{0});
			spawn(281213, 747.5162f, 514.51715f, 1011.692f, int8_t{0});
			spawn(281212, 726.8303f, 514.5155f, 1012.6845f, int8_t{0});
			spawn(281212, 727.9019f, 524.578f, 1012.68365f, int8_t{0});
			spawn(281212, 738.52844f, 525.0482f, 1011.692f, int8_t{0});
			spawn(281213, 758.3127f, 520.59143f, 1011.692f, int8_t{0});
			spawn(281213, 748.7474f, 525.84f, 1011.859f, int8_t{0});
			break;
		case 3:
			spawn(281212, 726.1167f, 503.28836f, 1012.6846f, int8_t{0});
			spawn(281212, 736.4446f, 505.3141f, 1012.1576f, int8_t{0});
			spawn(281213, 746.9261f, 503.50122f, 1012.68335f, int8_t{0});
			spawn(281212, 728.9705f, 492.59402f, 1012.68335f, int8_t{0});
			spawn(281212, 739.9526f, 491.54123f, 1011.692f, int8_t{0});
			spawn(281213, 749.754f, 491.74677f, 1011.8663f, int8_t{0});
			spawn(281213, 756.9996f, 500.01736f, 1011.692f, int8_t{0});
			spawn(281213, 736.9722f, 514.6446f, 1011.8599f, int8_t{0});
			spawn(281212, 747.5162f, 514.51715f, 1011.692f, int8_t{0});
			spawn(281213, 726.8303f, 514.5155f, 1012.6845f, int8_t{0});
			spawn(281213, 727.9019f, 524.578f, 1012.68365f, int8_t{0});
			spawn(281213, 738.52844f, 525.0482f, 1011.692f, int8_t{0});
			spawn(281212, 758.3127f, 520.59143f, 1011.692f, int8_t{0});
			spawn(281212, 748.7474f, 525.84f, 1011.859f, int8_t{0});
			break;
		default:
			break;
	}
}

} // namespace aion::gameserver::handlers::ai::instance::rakes
