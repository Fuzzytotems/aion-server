#include "aion/gameserver/handlers/admincommands/Speed.h"

#include "aion/commons/configuration/transformers/NumberTransformer.h"
#include "aion/gameserver/geoEngine/math/JavaFloat.h"
#include "aion/gameserver/handlers/admincommands/Stat.h"
#include "aion/gameserver/model/stats/container/CreatureGameStats.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(Speed);

Speed::Speed() : AdminCommand("speed", "Sets your speed.", "<0-100> - Set your speed to the specified value (0 to reset).\n") {
}

// Java Speed.java:24-43
void Speed::execute(Player& admin, std::span<const std::string> params) {
	using model::stats::calc::functions::IStatFunction;
	using model::stats::calc::functions::RcStatFunction;
	if (params.empty()) {
		sendInfo(admin);
		return;
	}
	float parameter = commons::configuration::transformers::NumberParser::parseFloat(params[0]); // Java: Float.parseFloat
	if (parameter < 0 || parameter > 100) {
		sendInfo(admin, "Speed must be between 0 and 100.");
		return;
	}
	admin.getGameStats()->endEffect(*this);
	if (parameter == 0) {
		sendInfo(admin, "Your regular speed has been restored.");
		return;
	}
	int32_t speed = geoEngine::math::JavaFloat::doubleToInt(parameter * 1000); // Java: (int) (parameter * 1000)
	// the Refs keep the new functions alive until addEffect stores its own
	runtime::Ref<RcStatFunction<Stat::CommandStatFunction>> speedFunction = RcStatFunction<Stat::CommandStatFunction>::create(StatEnum::SPEED, speed);
	runtime::Ref<RcStatFunction<Stat::CommandStatFunction>> flySpeedFunction =
		RcStatFunction<Stat::CommandStatFunction>::create(StatEnum::FLY_SPEED, speed);
	std::vector<runtime::Ptr<IStatFunction>> functions{speedFunction, flySpeedFunction};
	admin.getGameStats()->addEffect(runtime::Ptr<model::stats::calc::StatOwner>(this), functions);
	sendInfo(admin, "Your speed is now fixed at " + geoEngine::math::JavaFloat::toString(parameter) + ".");
}

} // namespace aion::gameserver::handlers::admincommands
