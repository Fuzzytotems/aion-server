#include "aion/gameserver/services/summons/SummonsService.h"

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/SummonController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/model/EmotionType.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/Summon.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/stats/container/SummonLifeStats.h"
#include "aion/gameserver/model/summons/SummonMode.h"
#include "aion/gameserver/model/summons/SummonRelease.h"
#include "aion/gameserver/model/summons/UnsummonType.h"
#include "aion/gameserver/model/summons/UnsummonTypeInfo.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SUMMON_OWNER_REMOVE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SUMMON_PANEL.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SUMMON_PANEL_REMOVE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SUMMON_UPDATE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/spawnengine/VisibleObjectSpawner.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

namespace aion::gameserver::services::summons {

using model::gameobjects::player::Player;
using network::aion::serverpackets::SM_EMOTION;
using network::aion::serverpackets::SM_SUMMON_OWNER_REMOVE;
using network::aion::serverpackets::SM_SUMMON_PANEL;
using network::aion::serverpackets::SM_SUMMON_PANEL_REMOVE;
using network::aion::serverpackets::SM_SUMMON_UPDATE;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using utils::PacketSendUtility;

// Anonymous classes and stored lambdas of the Java class (hub-headers.md §7.3): the bodies that create them define the structs that
// `python tools/gen/fieldmap.py --class <key>` prints.
//   com.aionemu.gameserver.services.summons.SummonsService.ReleaseSummonTask@L115:47

// Java implements Runnable. It schedules itself (schedule(this, ...), kept in SummonRelease.task) and keeps addedMasterHate for its run, so it
// is K4 (fieldmap.toml [kinds]): RefCounted, created with create(), retaining summon and release (a one-shot task: the pending Future is its
// only holder and releases it when it runs or is cancelled).
class SummonsService::ReleaseSummonTask final : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	const runtime::Ref<model::gameobjects::Summon> summon;
	const runtime::Ref<model::summons::SummonRelease> release_; // Java: release (renamed: RefCounted::release)
	const model::summons::UnsummonType unsummonType;
	runtime::Field<bool> addedMasterHate{false};

protected:
	ReleaseSummonTask(model::gameobjects::Summon& owner, model::summons::SummonRelease& release);
	~ReleaseSummonTask() override;

public:
	/** Java: new ReleaseSummonTask(owner, release) */
	static runtime::Ref<ReleaseSummonTask> create(model::gameobjects::Summon& owner, model::summons::SummonRelease& release);
	void run();
	void scheduleOrRun();
	void scheduleAddMasterHate(model::gameobjects::Summon& summon);
	std::vector<runtime::Ptr<controllers::attack::AggroList>> findSummonOnlyHaters(model::gameobjects::Summon& summon);
};

SummonsService::ReleaseSummonTask::ReleaseSummonTask(model::gameobjects::Summon& owner, model::summons::SummonRelease& value)
	: summon(owner), release_(value), unsummonType(value.getUnsummonType()) {
}

SummonsService::ReleaseSummonTask::~ReleaseSummonTask() = default;

runtime::Ref<SummonsService::ReleaseSummonTask> SummonsService::ReleaseSummonTask::create(model::gameobjects::Summon& owner,
	model::summons::SummonRelease& value) {
	return runtime::makeRef<ReleaseSummonTask>(owner, value);
}

void SummonsService::ReleaseSummonTask::run() {
	if (!summon->startRelease(*release_))
		return;
	Player& master = *runtime::cast<Player>(summon->getMaster());
	runtime::Ptr<model::gameobjects::VisibleObject> summonObj = world::World::getInstance().findVisibleObject(summon->getObjectId());
	// transformed npc via SM_TRANSFORM_IN_SUMMON
	if (runtime::Ptr<model::gameobjects::Npc> npc = runtime::as<model::gameobjects::Npc>(summonObj))
		npc->getController().delete_();
	else
		summon->getController().delete_(); // triggers SummonController.notKnow(master), the resulting DISTANCE release is ignored

	if (summon.get() == master.getSummon())
		master.setSummon(nullptr);

	const skillengine::model::SkillTemplate* summoningSkill = dataholders::DataManager::SKILL_DATA->getSkillTemplate(summon->getSummonedBySkillId());
	if (summoningSkill != nullptr && summoningSkill->getCooldown() > 0)
		master.setSkillCoolDown(summoningSkill->getCooldownId(), int64_t{summoningSkill->getCooldown() * 100} + commons::utils::currentTimeMillis());

	if (unsummonType == model::summons::UnsummonType::DISTANCE)
		PacketSendUtility::sendPacket(master, SM_SYSTEM_MESSAGE::STR_SKILL_SUMMON_UNSUMMON_BY_TOO_DISTANCE());
	else
		PacketSendUtility::sendPacket(master, SM_SYSTEM_MESSAGE::STR_SKILL_SUMMON_UNSUMMONED(summon->getL10n()));
	PacketSendUtility::sendPacket(master, SM_SUMMON_PANEL_REMOVE(summon->getSummonedBySkillId()));
	PacketSendUtility::sendPacket(master, SM_SUMMON_OWNER_REMOVE(summon->getObjectId()));
	if (!addedMasterHate.get())
		scheduleAddMasterHate(*summon);
}

void SummonsService::ReleaseSummonTask::scheduleOrRun() {
	if (model::summons::isInstant(unsummonType)) {
		run();
		return;
	}
	release_->setTask(utils::ThreadPoolManager::getInstance().schedule(
		runtime::bindTask([](ReleaseSummonTask& task) { task.run(); }, runtime::Ref<ReleaseSummonTask>(*this)),
		model::summons::getDelayMillis(unsummonType)));
	if (model::summons::isCancelableByMaster(unsummonType)) { // master hate is added delayed, he may still take the order back
		Player& master = *runtime::cast<Player>(summon->getMaster());
		PacketSendUtility::sendPacket(master, SM_SYSTEM_MESSAGE::STR_SKILL_SUMMON_UNSUMMON_FOLLOWER(summon->getL10n()));
		PacketSendUtility::sendPacket(master, SM_SUMMON_UPDATE(*summon));
	} else
		scheduleAddMasterHate(*summon);
}

void SummonsService::ReleaseSummonTask::scheduleAddMasterHate(model::gameobjects::Summon& value) {
	addedMasterHate.set(true);
	Player& master = *runtime::cast<Player>(value.getMaster());
	if (!master.isDead() && master.isOnline()) {
		std::vector<runtime::Ptr<controllers::attack::AggroList>> summonOnlyHaters = findSummonOnlyHaters(value);
		if (!summonOnlyHaters.empty()) { // add master hate to every npc which was only attacked by the summon before
			// C++: the task keeps Java's captured List<AggroList> as Refs (a Ref to a part retains its creature, Parts.h)
			std::vector<runtime::Ref<controllers::attack::AggroList>> haters(summonOnlyHaters.begin(), summonOnlyHaters.end());
			utils::ThreadPoolManager::getInstance().schedule({&value}, [&value, haters = std::move(haters)] {
				Player& taskMaster = *runtime::cast<Player>(value.getMaster());
				if (!taskMaster.isDead())
					for (const runtime::Ref<controllers::attack::AggroList>& aggroList : haters)
						aggroList->addHate(taskMaster, 1);
			}, 1000);
		}
	}
}

std::vector<runtime::Ptr<controllers::attack::AggroList>> SummonsService::ReleaseSummonTask::findSummonOnlyHaters(model::gameobjects::Summon& value) {
	std::vector<runtime::Ptr<controllers::attack::AggroList>> aggroLists;
	Player& master = *runtime::cast<Player>(value.getMaster());
	master.getKnownList().forEachObject([&value, &master, &aggroLists](model::gameobjects::VisibleObject& object) {
		if (runtime::Ptr<model::gameobjects::Creature> creature = runtime::as<model::gameobjects::Creature>(object)) {
			controllers::attack::AggroList& aggroList = creature->getAggroList();
			if (aggroList.isHating(value) && !aggroList.isHating(master))
				aggroLists.emplace_back(aggroList);
		}
	});
	return aggroLists;
}

runtime::Ptr<model::gameobjects::Summon> SummonsService::createSummon(model::gameobjects::player::Player& master, int32_t npcId, int32_t skillId,
	int32_t /*skillLevel*/, int32_t time) {
	if (master.getSummon()) {
		PacketSendUtility::sendPacket(master, SM_SYSTEM_MESSAGE::STR_SKILL_SUMMON_ALREADY_HAVE_A_FOLLOWER());
		return nullptr;
	}
	runtime::Ref<model::gameobjects::Summon> summon = spawnengine::VisibleObjectSpawner::spawnSummon(master, npcId, skillId, time);
	master.setSummon(summon);
	PacketSendUtility::sendPacket(master, SM_SUMMON_PANEL(*summon));
	PacketSendUtility::broadcastPacket(*summon, SM_EMOTION(*summon, model::EmotionType::CHANGE_SPEED));
	PacketSendUtility::broadcastPacket(*summon, SM_SUMMON_UPDATE(*summon));
	return summon;
}

void SummonsService::release(model::gameobjects::Summon& summon, model::summons::UnsummonType unsummonType) {
	runtime::Ref<model::summons::SummonRelease> release = model::summons::SummonRelease::create(unsummonType);
	if (!summon.registerRelease(*release))
		return;
	summon.getController().cancelCurrentSkill(nullptr);
	summon.setMode(model::summons::SummonMode::RELEASE);
	summon.getObserveController()->notifySummonReleaseObservers();
	ReleaseSummonTask::create(summon, *release)->scheduleOrRun();
}

void SummonsService::restMode(model::gameobjects::Summon& summon) {
	summon.getController().cancelCurrentSkill(nullptr);
	summon.setMode(model::summons::SummonMode::REST);
	Player& master = *runtime::cast<Player>(summon.getMaster());
	PacketSendUtility::sendPacket(master, SM_SYSTEM_MESSAGE::STR_SKILL_SUMMON_REST_MODE(summon.getL10n()));
	PacketSendUtility::sendPacket(master, SM_SUMMON_UPDATE(summon));
	summon.getLifeStats()->triggerRestoreTask();
}

void SummonsService::setUnkMode(model::gameobjects::Summon& summon) {
	summon.setMode(model::summons::SummonMode::UNK);
	Player& master = *runtime::cast<Player>(summon.getMaster());
	PacketSendUtility::sendPacket(master, SM_SUMMON_UPDATE(summon));
}

void SummonsService::guardMode(model::gameobjects::Summon& summon) {
	summon.getController().cancelCurrentSkill(nullptr);
	summon.setMode(model::summons::SummonMode::GUARD);
	Player& master = *runtime::cast<Player>(summon.getMaster());
	PacketSendUtility::sendPacket(master, SM_SYSTEM_MESSAGE::STR_SKILL_SUMMON_GUARD_MODE(summon.getL10n()));
	PacketSendUtility::sendPacket(master, SM_SUMMON_UPDATE(summon));
	summon.getLifeStats()->triggerRestoreTask();
}

void SummonsService::attackMode(model::gameobjects::Summon& summon) {
	summon.setMode(model::summons::SummonMode::ATTACK);
	Player& master = *runtime::cast<Player>(summon.getMaster());
	PacketSendUtility::sendPacket(master, SM_SYSTEM_MESSAGE::STR_SKILL_SUMMON_ATTACK_MODE(summon.getL10n()));
	PacketSendUtility::sendPacket(master, SM_SUMMON_UPDATE(summon));
	summon.getLifeStats()->cancelRestoreTask();
}

void SummonsService::doMode(model::summons::SummonMode summonMode, model::gameobjects::Summon& summon) {
	doMode(summonMode, summon, 0, std::nullopt);
}

void SummonsService::doMode(model::summons::SummonMode summonMode, model::gameobjects::Summon& summon, model::summons::UnsummonType unsummonType) {
	doMode(summonMode, summon, 0, unsummonType);
}

void SummonsService::doMode(model::summons::SummonMode summonMode, model::gameobjects::Summon& summon, int32_t targetObjId,
	std::optional<model::summons::UnsummonType> unsummonType) {
	using model::summons::SummonMode;
	if (summon.isDead())
		return;

	if (!summon.getMaster())
		return;

	if (unsummonType == model::summons::UnsummonType::COMMAND) {
		if (summon.isReleaseUncancelable())
			return;
		if (summonMode == SummonMode::ATTACK && !summon.getController().canAttack(targetObjId))
			return; // don't cancel a pending release for an order that won't be carried out
		// UNK leaves the summons mode untouched, so it must not take back a pending release either
		if (summonMode == SummonMode::ATTACK || summonMode == SummonMode::GUARD || summonMode == SummonMode::REST)
			summon.cancelReleaseByMaster();
	}

	switch (summonMode) {
		case SummonMode::REST:
			summon.getController().restMode();
			break;
		case SummonMode::ATTACK:
			summon.getController().attackMode(targetObjId);
			break;
		case SummonMode::GUARD:
			summon.getController().guardMode();
			break;
		case SummonMode::RELEASE:
			if (unsummonType) {
				summon.getController().release(*unsummonType);
			}
			break;
		case SummonMode::UNK:
			break;
	}
}

} // namespace aion::gameserver::services::summons
