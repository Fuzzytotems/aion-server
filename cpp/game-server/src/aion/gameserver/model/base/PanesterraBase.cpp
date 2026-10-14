#include "aion/gameserver/model/base/PanesterraBase.h"

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/model/base/PanesterraBaseLocation.h"
#include "aion/gameserver/model/base/BaseOccupier.h"
#include "aion/gameserver/model/base/BaseOccupierInfo.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/base/BaseTemplate.h"

namespace aion::gameserver::model::base {

// Java PanesterraBase.java:14-16
PanesterraBase::PanesterraBase(PanesterraBaseLocation& loc) : Base(loc) {
}

runtime::Ref<PanesterraBase> PanesterraBase::create(PanesterraBaseLocation& loc) {
	return runtime::makeRef<PanesterraBase>(loc);
}

runtime::Ptr<PanesterraBaseLocation> PanesterraBase::getLocation() const {
	return runtime::cast<PanesterraBaseLocation>(Base::getLocation());
}

// Java PanesterraBase.java:18-21
int32_t PanesterraBase::getAssaultDelay() {
	return commons::utils::Rnd::get(75, 200) * 6000;
}

// Java PanesterraBase.java:23-26
int32_t PanesterraBase::getAssaultDespawnDelay() {
	return 15 * 60000; // Retail delay
}

// Java PanesterraBase.java:28-31
int32_t PanesterraBase::getBossSpawnDelay() {
	return 20 * 60000; // Retail delay
}

// Java PanesterraBase.java:33-36
int32_t PanesterraBase::getNpcSpawnDelay() {
	return 5 * 60000; // Retail delay
}

// Java PanesterraBase.java:38-41
BaseOccupier PanesterraBase::chooseAssaultRace() {
	return BaseOccupier::BALAUR;
}

// Java PanesterraBase.java:43-48
BaseOccupier PanesterraBase::getOccupier(runtime::Ptr<gameobjects::Creature> bossKiller) {
	if (runtime::Ptr<gameobjects::player::Player> player = runtime::as<gameobjects::player::Player>(bossKiller); player && player->getPanesterraFaction())
		return findBy(*player->getPanesterraFaction());
	return getLocation()->getTemplate()->getDefaultOccupier();
}

PanesterraBase::~PanesterraBase() = default;

} // namespace aion::gameserver::model::base
