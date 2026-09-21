#include "aion/gameserver/skillengine/task/GatheringTask.h"

#include <algorithm>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/configs/main/CraftConfig.h"
#include "aion/gameserver/controllers/GatherableController.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/observer/ActionObserver.h"
#include "aion/gameserver/controllers/observer/ObserverType.h"
#include "aion/gameserver/instance/handlers/InstanceHandler.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Gatherable.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/Rates.h"
#include "aion/gameserver/model/gameobjects/player/RatesInfo.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/gather/GatherableTemplate.h"
#include "aion/gameserver/model/templates/gather/Material.h"
#include "aion/gameserver/network/aion/serverpackets/SM_GATHER_ANIMATION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_GATHER_UPDATE.h"
#include "aion/gameserver/services/item/ItemService.h"
#include "aion/gameserver/skillengine/task/AbstractCraftTask_CraftTypeInfo.h"
#include "aion/gameserver/utils/JavaMath.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::skillengine::task {

using gameserver::model::gameobjects::Gatherable;
using network::aion::serverpackets::SM_GATHER_ANIMATION;
using network::aion::serverpackets::SM_GATHER_UPDATE;
using utils::PacketSendUtility;

/**
 * Java: the anonymous ActionObserver of GatheringTask.createGathererObserver (fieldmap.py --class
 * 'com.aionemu.gameserver.skillengine.task.GatheringTask$1'), stored in gathererObserver and in the gatherer's ObserveController. Every
 * event it listens to aborts the gathering.
 */
struct GatheringTask_ActionObserver final : controllers::observer::ActionObserver {
	AION_MAKE_REF_FRIEND

	const runtime::Ref<GatheringTask> gatheringTask; // captured this GatheringTask this (GatheringTask.java:141)

	static runtime::Ref<GatheringTask_ActionObserver> create(GatheringTask& gatheringTask) {
		return runtime::makeRef<GatheringTask_ActionObserver>(gatheringTask);
	}

	void startSkillCast(model::Skill& /*skill*/) override { gatheringTask->abort(); }

	void attack(gameserver::model::gameobjects::Creature& /*creature*/, int32_t /*skillId*/) override { gatheringTask->abort(); }

	void attacked(gameserver::model::gameobjects::Creature& /*creature*/, int32_t /*skillId*/) override { gatheringTask->abort(); }

	void moved() override { gatheringTask->abort(); }

	void dotattacked(gameserver::model::gameobjects::Creature& /*creature*/, model::Effect& /*dotEffect*/) override { gatheringTask->abort(); }

protected:
	explicit GatheringTask_ActionObserver(GatheringTask& gatheringTaskValue)
		: ActionObserver(controllers::observer::ObserverType::ALL), gatheringTask(runtime::Ref<GatheringTask>(gatheringTaskValue)) {}
	~GatheringTask_ActionObserver() override = default;
};

GatheringTask::GatheringTask(gameserver::model::gameobjects::player::Player& requesterValue, Gatherable& gatherable,
	const gameserver::model::templates::gather::Material* materialValue, int32_t skillLvlDiffValue)
	: AbstractCraftTask(requesterValue, gatherable, skillLvlDiffValue), template_(gatherable.getObjectTemplate()), material(materialValue) {
	// Java also assigns gathererObserver here; create() does it (see the class comment).
	delay.set(commons::utils::Rnd::get(200, 600));
	int32_t gatherInterval = 2500 - (skillLvlDiffValue * 60);
	interval.set(gatherInterval < 1200 ? 1200 : gatherInterval);
}

GatheringTask::~GatheringTask() = default;

runtime::Ref<GatheringTask> GatheringTask::create(gameserver::model::gameobjects::player::Player& requesterValue, Gatherable& gatherable,
	const gameserver::model::templates::gather::Material* materialValue, int32_t skillLvlDiffValue) {
	runtime::Ref<GatheringTask> task = runtime::makeRef<GatheringTask>(requesterValue, gatherable, materialValue, skillLvlDiffValue);
	// C++ only: Java's constructor calls createGathererObserver(), which captures `this`. A Ref to a RefCounted cannot be taken inside its own
	// constructor, so the observer is created here, before anyone else can see the task.
	task->gathererObserver.set(task->createGathererObserver());
	return task;
}

void GatheringTask::onInteractionAbort() {
	PacketSendUtility::broadcastPacket(*requester,
		SM_GATHER_ANIMATION(requester->getObjectId(), responder->getObjectId(), template_->getHarvestSkill(), 4));
	PacketSendUtility::sendPacket(*requester, SM_GATHER_UPDATE(template_, material, 0, 0, 5, 0, 0));
}

void GatheringTask::onInteractionFinish() {
	// Java: requester.getObserveController().removeObserver(gathererObserver) - the field is final there and never null. The C++ field is
	// cleared right after the removal (cycles.toml cpp-breaker), so a second finish (abort of an already stopped task) finds it empty.
	if (runtime::Ptr<controllers::observer::ActionObserver> observer = gathererObserver.get()) {
		requester->getObserveController()->removeObserver(*observer);
		gathererObserver.set(nullptr);
	}
	static_cast<Gatherable&>(*responder).getController().completeInteraction();
}

void GatheringTask::onInteractionStart() {
	// Java's gathererObserver is final and always non-null, so a restarted task re-attaches the same observer. onInteractionFinish clears the
	// C++ field (cycles.toml cpp-breaker), so a restart recreates it here instead of dereferencing an empty Ref.
	if (!gathererObserver.get())
		gathererObserver.set(createGathererObserver());
	requester->getObserveController()->attach(*gathererObserver.get());
	PacketSendUtility::sendPacket(*requester, SM_GATHER_UPDATE(template_, material, fullBarValue, fullBarValue, 0, 0, 0));
	PacketSendUtility::sendPacket(*requester, SM_GATHER_UPDATE(template_, material, 0, 0, 1, 0, 0));
	// TODO: missing packet for initial failure/success
	PacketSendUtility::broadcastPacket(*requester,
		SM_GATHER_ANIMATION(requester->getObjectId(), responder->getObjectId(), template_->getHarvestSkill(), 0), true);
	PacketSendUtility::broadcastPacket(*requester,
		SM_GATHER_ANIMATION(requester->getObjectId(), responder->getObjectId(), template_->getHarvestSkill(), 1), true);
}

void GatheringTask::sendInteractionUpdate() {
	PacketSendUtility::sendPacket(*requester, SM_GATHER_UPDATE(template_, material, currentSuccessValue.get(), currentFailureValue.get(),
											 getProgressId(craftType.get()), executionSpeed.get(), showBarDelay.get()));
}

void GatheringTask::onFailureFinish() {
	PacketSendUtility::sendPacket(*requester,
		SM_GATHER_UPDATE(template_, material, currentSuccessValue.get(), currentFailureValue.get(), 1, 0, 0));
	PacketSendUtility::sendPacket(*requester,
		SM_GATHER_UPDATE(template_, material, currentSuccessValue.get(), currentFailureValue.get(), 7, 0, 0));
	PacketSendUtility::broadcastPacket(*requester,
		SM_GATHER_ANIMATION(requester->getObjectId(), responder->getObjectId(), template_->getHarvestSkill(), 3), true);
}

bool GatheringTask::onSuccessFinish() {
	PacketSendUtility::broadcastPacket(*requester,
		SM_GATHER_ANIMATION(requester->getObjectId(), responder->getObjectId(), template_->getHarvestSkill(), 2), true);
	PacketSendUtility::sendPacket(*requester,
		SM_GATHER_UPDATE(template_, material, currentSuccessValue.get(), currentFailureValue.get(), 6, 0, 0));
	if (template_->getEraseValue() > 0)
		requester->getInventory().decreaseByItemId(template_->getRequiredItemId(), template_->getEraseValue());
	services::item::ItemService::addItem(*requester, material->getItemId(),
		calcResult(gameserver::model::gameobjects::player::Rates::GATHERING_COUNT, *requester, 1));
	requester->getPosition()->getWorldMapInstance()->getInstanceHandler()->onGather(*requester, static_cast<Gatherable&>(*responder));
	static_cast<Gatherable&>(*responder).getController().rewardPlayer(requester);
	return true;
}

void GatheringTask::analyzeInteraction() {
	const int32_t diff = skillLvlDiff.get();
	if (diff >= 41) {
		currentSuccessValue.set(fullBarValue);
		executionSpeed.set(300);
		showBarDelay.set(500);
		return;
	} else if (diff < 0) {
		currentFailureValue.set(fullBarValue);
		return;
	}

	craftType.set(CraftType::NORMAL);
	float multi = commons::utils::Rnd::nextFloat(1.0f, 2.0f);
	float failReduction = std::max(1.0f - static_cast<float>(diff) * 0.015f, 0.25f); // dynamic fail rate multiplier
	bool success = commons::utils::Rnd::chance()
		>= static_cast<float>(configs::main::CraftConfig::MAX_GATHER_FAILURE_CHANCE.load()) * failReduction;

	if (success) {
		float critChance = commons::utils::Rnd::chance();
		if (critChance < (1.0f + static_cast<float>(diff) / 10.0f)) { // PURPLE CRIT = 100%
			craftType.set(CraftType::CRIT_PURPLE);
			currentSuccessValue.set(fullBarValue);
			executionSpeed.set(300);
			showBarDelay.set(500);
			return;
		} else if (critChance < (5.0f + static_cast<float>(diff) / 3.0f)) { // LIGHT BLUE CRIT = +10%
			craftType.set(CraftType::CRIT_BLUE);
		}

		int32_t lvlBoni = diff > 10 ? ((diff - 10) * 2) : 0;
		float gain = (craftType.get() == CraftType::CRIT_BLUE ? 100.0f : 0.0f)
			+ ((static_cast<float>(diff + 1) / 2.0f) + static_cast<float>(lvlBoni)) * 10.0f;
		currentSuccessValue.set(currentSuccessValue.get() + utils::JavaMath::round(70.0f + gain * multi));
	} else {
		currentFailureValue.set(
			currentFailureValue.get() + utils::JavaMath::round(120.0f + (static_cast<float>(diff + 1) / 2.0f * 10.0f) * multi));
	}

	if (currentSuccessValue.get() > fullBarValue) {
		currentSuccessValue.set(fullBarValue);
	} else if (currentFailureValue.get() > fullBarValue) {
		currentFailureValue.set(fullBarValue);
	}

	int32_t speed = 900 - (diff * 30);
	executionSpeed.set(speed < 300 ? 300 : speed);
	showBarDelay.set(std::max(500, 1200 - (diff * 30)));
}

int32_t GatheringTask::getGathererId() {
	return requester->getObjectId();
}

runtime::Ref<controllers::observer::ActionObserver> GatheringTask::createGathererObserver() {
	return GatheringTask_ActionObserver::create(*this);
}

} // namespace aion::gameserver::skillengine::task
