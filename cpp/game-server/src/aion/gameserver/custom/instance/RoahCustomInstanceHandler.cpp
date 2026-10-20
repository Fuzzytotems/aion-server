#include "aion/gameserver/custom/instance/RoahCustomInstanceHandler.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <optional>
#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Rnd.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/attack/AggroInfo.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/controllers/effect/PlayerEffectController.h"
#include "aion/gameserver/custom/instance/CustomInstanceRankEnumInfo.h"
#include "aion/gameserver/custom/instance/CustomInstanceService.h"
#include "aion/gameserver/custom/instance/neuralnetwork/PlayerModel.h"
#include "aion/gameserver/custom/instance/neuralnetwork/PlayerModelController.h"
#include "aion/gameserver/dao/PlayerDAO.h"
#include "aion/gameserver/model/ChatType.h"
#include "aion/gameserver/model/Gender.h"
#include "aion/gameserver/model/PlayerClassInfo.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/drop/DropItem.h"
#include "aion/gameserver/model/flyring/FlyRing.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/geometry/Point3D.h"
#include "aion/gameserver/model/stats/calc/functions/StatFunction.h"
#include "aion/gameserver/model/stats/calc/functions/StatSetFunction.h"
#include "aion/gameserver/model/stats/container/CreatureGameStats.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/model/stats/container/NpcGameStats.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/model/templates/flyring/FlyRingTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MESSAGE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUEST_ACTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/drop/DropRegistrationService.h"
#include "aion/gameserver/services/event/EventService.h"
#include "aion/gameserver/services/item/ItemService.h"
#include "aion/gameserver/services/player/PlayerService.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/world/WorldMapInstance.h"

namespace aion::gameserver::custom::instance {

using model::ChatType;
using model::gameobjects::Npc;
using model::gameobjects::player::Player;
using network::aion::serverpackets::SM_MESSAGE;
using network::aion::serverpackets::SM_QUEST_ACTION;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using utils::PacketSendUtility;

namespace {

/** Java: (Npc) spawn(...) - a ClassCastException is impossible here (the ids are npcs); null stays null */
runtime::Ptr<Npc> asNpc(runtime::Ptr<model::gameobjects::VisibleObject> object) {
	return runtime::as<Npc>(object);
}

} // namespace

// Java RoahCustomInstanceHandler.java:73-75
RoahCustomInstanceHandler::RoahCustomInstanceHandler(world::WorldMapInstance& instance)
	: GeneralInstanceHandler(instance), dpsReport(std::make_unique<RoahDpsReport>([] { return commons::utils::currentTimeMillis(); })) {
}

runtime::Ref<RoahCustomInstanceHandler> RoahCustomInstanceHandler::create(world::WorldMapInstance& instance) {
	return runtime::makeRef<RoahCustomInstanceHandler>(instance);
}

// Java RoahCustomInstanceHandler.java:77-81
void RoahCustomInstanceHandler::onInstanceCreate() {
	spawnRings();
	spawn(730588, 505.2431f, 377.0414f, 93.8944f, int8_t{30}, 33); // Exit
}

// Java RoahCustomInstanceHandler.java:83-86
void RoahCustomInstanceHandler::onInstanceDestroy() {
	cancelAllTasks();
}

// Java RoahCustomInstanceHandler.java:88-92 (the template is immortal, as static data: FlyRingTemplate.h)
void RoahCustomInstanceHandler::spawnRings() {
	// transparent entry gate
	runtime::Ref<model::geometry::Point3D> center = model::geometry::Point3D::create(501.77, 409.53, 94.12);
	runtime::Ref<model::geometry::Point3D> p1 = model::geometry::Point3D::create(503.93, 409.65, 98.9);
	runtime::Ref<model::geometry::Point3D> p2 = model::geometry::Point3D::create(506.26, 409.7, 94.15);
	const auto* flyRingTemplate = new model::templates::flyring::FlyRingTemplate("ROAH_WING_1", mapId, *center, *p1, *p2, 10);
	model::gameobjects::VisibleObject::create<model::flyring::FlyRing>(flyRingTemplate, instance->getInstanceId())->spawn();
}

// Java RoahCustomInstanceHandler.java:94-175
bool RoahCustomInstanceHandler::onPassFlyingRing(Player& player, std::string_view flyingRing) {
	if (flyingRing == "ROAH_WING_1" && startTime.compareAndSet(0, commons::utils::currentTimeMillis())) {
		dpsReport->runStarted(); // C++-dev only: the DPS report's run starts with the run
		runtime::Ptr<Npc> artifact = asNpc(spawn(CENTER_ARTIFACT_ID, 504.1977f, 481.5051f, 87.2790f, int8_t{30}));
		if (artifact != nullptr)
			adaptNPC(*artifact, rank.get());

		PacketSendUtility::broadcastToMap(*instance, SM_SYSTEM_MESSAGE::STR_MSG_INSTANCE_START_IDABRE());
		PacketSendUtility::broadcastToMap(*instance, SM_QUEST_ACTION(0, TIME_LIMIT));
		PacketSendUtility::broadcastToMap(*instance,
			SM_MESSAGE(0, "", "You have 15 minutes to demolish the central Aether Stone. Beware for its protectors!", ChatType::BRIGHT_YELLOW_CENTER));

		despawnTask.set(utils::ThreadPoolManager::getInstance().schedule({this}, [this] {
			setResult(false, "time limit reached"); // Java: setResult(false); C++-dev: the reason for the DPS report
			deleteAliveNpcs({CENTER_ARTIFACT_ID, TRASH_MOB_ID, BULKY_MOB_ID, DOMINATOR_MOB_ID, BOSS_MOB_A_M_ID, BOSS_MOB_A_F_ID, BOSS_MOB_E_M_ID,
				BOSS_MOB_E_F_ID, BOSS_MOB_AT_ID});
		}, TIME_LIMIT * 1000));

		runtime::Ref<Player> hated(player); // Java: the lambdas capture player
		// spawn bulky mob: 1/min after 1:20min
		bulkyMobSpawnTask.set(utils::ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this, hated] {
			runtime::Ptr<Npc> bulky = nullptr;
			for (runtime::Ptr<Npc> n : instance->getNpcs({BULKY_MOB_ID})) {
				if (!n->isDead() && !n->getLifeStats()->isAboutToDie()) {
					bulky = n;
					break;
				}
			}
			if (bulky != nullptr) {
				bulky->getLifeStats()->setCurrentHp(bulky->getLifeStats()->getMaxHp());
				PacketSendUtility::broadcastToMap(*instance, SM_MESSAGE(0, "", "Protector Vord recovered energy!", ChatType::BRIGHT_YELLOW_CENTER));
			} else {
				bulky = asNpc(spawn(BULKY_MOB_ID, 493.923f, 488.16794f, 87.18341f, int8_t{0}));
				adaptNPC(*bulky, rank.get());
				bulky->getAggroList().addHate(*hated, 1000);
				PacketSendUtility::broadcastToMap(*instance, SM_MESSAGE(0, "", "A tough protector has appeared!", ChatType::BRIGHT_YELLOW_CENTER));
			}
		}, 80000, 60000));

		if (rank.get() >= getMinRank(CustomInstanceRankEnum::BRONZE)) {
			// spawn trash mobs: 1 wave/min after 1min
			trashMobSpawnTask.set(utils::ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this, hated] {
				int32_t newTrash = 6;
				for (runtime::Ptr<Npc> n : instance->getNpcs({TRASH_MOB_ID})) // always spawn until 6 total
					if (!n->isDead())
						newTrash--;

				if (newTrash > 0) {
					for (int32_t i = 0; i < newTrash; i++) {
						runtime::Ptr<Npc> mob = asNpc(spawn(TRASH_MOB_ID, 500.1f + commons::utils::Rnd::nextFloat(8.0f),
							460.0f + commons::utils::Rnd::nextFloat(4.0f), 86.7112f, int8_t{30}));
						adaptNPC(*mob, rank.get());
						mob->getAggroList().addHate(*hated, 1000);
					}
				}
			}, 60000, 60000 - rank.get() * int64_t{1000})); // 60s ... 30s
		}

		if (rank.get() >= getMinRank(CustomInstanceRankEnum::SILVER)) {
			// spawn dominator mob: 1/min after 1:40min
			dominatorMobSpawnTask.set(utils::ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this, hated] {
				runtime::Ptr<Npc> dominator = nullptr;
				for (runtime::Ptr<Npc> n : instance->getNpcs({DOMINATOR_MOB_ID})) {
					if (!n->isDead() && !n->getLifeStats()->isAboutToDie()) {
						dominator = n;
						break;
					}
				}
				if (dominator != nullptr) {
					dominator->getLifeStats()->setCurrentHp(dominator->getLifeStats()->getMaxHp());
					PacketSendUtility::broadcastToMap(*instance, SM_MESSAGE(0, "", "Protector Vala recovered energy!", ChatType::BRIGHT_YELLOW_CENTER));

				} else {
					dominator = asNpc(spawn(DOMINATOR_MOB_ID, 515.23114f, 487.94998f, 87.176056f, int8_t{60}));
					adaptNPC(*dominator, rank.get());
					dominator->getAggroList().addHate(*hated, 1000);
					PacketSendUtility::broadcastToMap(*instance, SM_MESSAGE(0, "", "A vile protector has appeared!", ChatType::BRIGHT_YELLOW_CENTER));
				}
			}, 100000, 60000));
		}
	}
	return false;
}

// Java RoahCustomInstanceHandler.java:177-221
void RoahCustomInstanceHandler::onDie(Npc& npc) {
	dpsReport->addDamage(playerDamageOn(npc)); // C++-dev only: every npc's damage counts for the phase's total DPS
	switch (npc.getNpcId()) {
		case TRASH_MOB_ID:
		case BULKY_MOB_ID:
		case DOMINATOR_MOB_ID:
			npc.getController().delete_();
			break;
		case CENTER_ARTIFACT_ID: {
			cancelAllTasks();
			// use pcd for rare cases in which the artifact got destroyed by dots/servants and the player got DC'ed before
			runtime::Ref<model::gameobjects::player::PlayerCommonData> pcd = services::player::PlayerService::getOrLoadPlayerCommonData(playerObjId.get());
			float usedTime = static_cast<float>(commons::utils::currentTimeMillis() - startTime.get()) / 1000.0f;
			achievedDps.set(static_cast<int32_t>(std::floor(static_cast<float>(npc.getLifeStats()->getMaxHp()) / usedTime + 0.5f))); // Java: Math.round(float)
			log.info(std::format(
				"[CI_ROAH] Player [id={}, name={}, class={}, rank={}({}), isPrebuffed={}] succeeded in destroying the artifact in {:.3f}s (DPS:{}).",
				pcd->getPlayerObjId(), pcd->getName(), xml::enumName(pcd->getPlayerClass()), getRankDescription(rank.get()), rank.get(),
				isPrebuffed.get(), usedTime, achievedDps.get()));
			addDamageOnAliveNpcs(&npc); // C++-dev only: the protectors deleted below still count for the phase
			dpsReport->artifactDestroyed(usedTime, achievedDps.get(), avgClassDps.get(), isPrebuffed.get()); // C++-dev only: the artifact phase
			sendReport(dpsReport->artifactMessage());
			deleteAliveNpcs({TRASH_MOB_ID, BULKY_MOB_ID, DOMINATOR_MOB_ID});
			PacketSendUtility::broadcastToMap(*instance, SM_MESSAGE(0, "", "You feel a shadowy presence from the throne room.", ChatType::BRIGHT_YELLOW_CENTER));
			PacketSendUtility::broadcastToMap(*instance, SM_QUEST_ACTION(0, 0));
			int32_t bossID = BOSS_MOB_AT_ID;
			if (pcd->getPlayerClass() != model::PlayerClass::RIDER) {
				switch (pcd->getRace()) {
					case model::Race::ASMODIANS:
						bossID = pcd->getGender() == model::Gender::MALE ? BOSS_MOB_A_M_ID : BOSS_MOB_A_F_ID;
						break;
					case model::Race::ELYOS:
						bossID = pcd->getGender() == model::Gender::MALE ? BOSS_MOB_E_M_ID : BOSS_MOB_E_F_ID;
						break;
					default:
						break;
				}
			}
			spawn(bossID, 503.96774f, 631.935f, 104.548775f, int8_t{90}); // spawn bossMob
			dpsReport->bossSpawned(); // C++-dev only: the boss phase starts
			isBossPhase.set(true);
			npc.getController().delete_();
			break;
		}
		case BOSS_MOB_A_M_ID:
		case BOSS_MOB_A_F_ID:
		case BOSS_MOB_E_M_ID:
		case BOSS_MOB_E_F_ID:
		case BOSS_MOB_AT_ID: {
			closeRunDamage(&npc); // C++-dev only: adds still alive count for the boss phase
			dpsReport->bossKilled(npc.getLifeStats()->getMaxHp()); // C++-dev only: the boss phase, logged in the [CI_ROAH] style
			runtime::Ref<model::gameobjects::player::PlayerCommonData> pcd = services::player::PlayerService::getOrLoadPlayerCommonData(playerObjId.get());
			log.info(std::format("[CI_ROAH] Player [id={}, name={}, class={}, rank={}({}), isPrebuffed={}] killed the boss in {:.3f}s (DPS:{}, total DPS:{}), "
				"the whole run took {:.3f}s (total DPS:{}).", pcd->getPlayerObjId(), pcd->getName(), xml::enumName(pcd->getPlayerClass()),
				getRankDescription(rank.get()), rank.get(), isPrebuffed.get(), dpsReport->getBossSeconds(), dpsReport->getBossDps(),
				dpsReport->getBossPhaseTotalDps(), dpsReport->getTotalSeconds(), dpsReport->getRunTotalDps()));
			calcBossDrop(npc.getObjectId());
			setResult(true);
			PacketSendUtility::broadcastToMap(*instance, SM_QUEST_ACTION(0, 0));
			break;
		}
		default:
			break;
	}
}

// Java RoahCustomInstanceHandler.java:223-237
void RoahCustomInstanceHandler::onEnterInstance(Player& player) {
	if (isInitialized.compareAndSet(false, true)) {
		isPrebuffed.set(checkForPrebuffOnEntry(player)); // check for Word of Wind and Symphony of Destruction
		setAverageDpsByPlayerClass(player.getPlayerClass());
		playerObjId.set(instance->getRegisteredObjects().iterator().next());
		rank.set(CustomInstanceService::getInstance().loadOrCreateRank(playerObjId.get()).getRank());
		PacketSendUtility::broadcastToMap(*instance, SM_MESSAGE(0, "",
			"Welcome to the 'Eternal Challenge', " + getRankDescription(rank.get()) + " challenger!", ChatType::BRIGHT_YELLOW_CENTER));
		spawnUnlockableNpcs(player);
		// train player model from previous boss runs
		std::vector<int32_t> skills = neuralnetwork::PlayerModelController::getSkillSetForPlayer(playerObjId.get());
		runtime::Ref<runtime::RcArrayList<int32_t>> skillList = runtime::RcArrayList<int32_t>::create(AION_LOCK_CLASS(RoahCustomInstanceHandler::skillSet));
		for (int32_t skill : skills)
			skillList->add(skill);
		skillSet.set(skillList);
		model.set(neuralnetwork::PlayerModelController::trainModelForPlayer(playerObjId.get(), skills));
	}
}

// C++-dev only, not in Java: a run left before its end gets its failure report (the rank is not changed: Java's run just ends)
void RoahCustomInstanceHandler::onLeaveInstance(Player& player) {
	if (dpsReport->isRunStarted() && !isCompleted.get()) {
		closeRunDamage(nullptr);
		dpsReport->runEnded();
		PacketSendUtility::sendMessage(player, dpsReport->failureReport("left the instance"));
	}
	GeneralInstanceHandler::onLeaveInstance(player);
}

// Java RoahCustomInstanceHandler.java:239-241
bool RoahCustomInstanceHandler::checkForPrebuffOnEntry(Player& player) {
	return player.getEffectController()->findBySkillId(1810) != nullptr || player.getEffectController()->findBySkillId(4469) != nullptr;
}

// Java RoahCustomInstanceHandler.java:243-251
void RoahCustomInstanceHandler::setAverageDpsByPlayerClass(model::PlayerClass playerClass) {
	switch (playerClass) {
		case model::PlayerClass::TEMPLAR:
		case model::PlayerClass::CHANTER:
			avgClassDps.set(2500);
			break;
		case model::PlayerClass::GUNNER:
			avgClassDps.set(4500);
			break;
		case model::PlayerClass::SORCERER:
		case model::PlayerClass::ASSASSIN:
			avgClassDps.set(5000);
			break;
		case model::PlayerClass::SPIRIT_MASTER:
			avgClassDps.set(6000);
			break;
		default:
			avgClassDps.set(4000);
			break;
	}
}

// Java RoahCustomInstanceHandler.java:253-263
void RoahCustomInstanceHandler::calcBossDrop(int32_t npcObjId) {
	services::drop::DropRegistrationService& drops = services::drop::DropRegistrationService::getInstance();
	runtime::Ptr<runtime::RcHashSet<runtime::Ref<model::drop::DropItem>>> dropItems = drops.getCurrentDropMap().get(npcObjId);
	if (dropItems != nullptr) { // dropItems can possibly be null if the player died right before the boss
		dropItems->clear();
		dropItems->add(drops.regDropItem(0, playerObjId.get(), npcObjId, CustomInstanceService::REWARD_COIN_ID, getRewardCoinAmount(rank.get())));

		if (services::event::EventService::getInstance().isEventActive("Ice Festival"))
			dropItems->add(drops.regDropItem(0, playerObjId.get(), npcObjId, 182215803, 1));
	}
}

// Java RoahCustomInstanceHandler.java:265-271
int32_t RoahCustomInstanceHandler::getRewardCoinAmount(int32_t rank) {
	CustomInstanceRankEnum rankEnum = getByRank(rank);
	if (rankEnum != CustomInstanceRankEnum::ANCIENT_PLUS)
		return getMinReward(rankEnum);
	else
		return static_cast<int32_t>(static_cast<float>(getMinReward(rankEnum)) + static_cast<float>(rank - getMinRank(rankEnum)) * REWARD_SCALE);
}

// Java RoahCustomInstanceHandler.java:273-299
void RoahCustomInstanceHandler::adaptNPC(Npc& npc, int32_t rank) {
	using model::stats::calc::functions::RcStatFunction;
	using model::stats::calc::functions::StatSetFunction;
	using model::stats::container::StatEnum;
	std::vector<runtime::Ref<model::stats::calc::functions::IStatFunction>> functions;
	const int32_t avgDps = avgClassDps.get();

	switch (npc.getNpcId()) { // IRON ... ANCIENT+
		case CENTER_ARTIFACT_ID: { // ~5 ... 10min
			int32_t maxHP = 300 * avgDps + rank * 10 * avgDps;
			if (rank > getMinRank(CustomInstanceRankEnum::ANCIENT))
				maxHP += (rank - getMinRank(CustomInstanceRankEnum::ANCIENT)) * 150000;
			functions.push_back(RcStatFunction<StatSetFunction>::create(StatEnum::MAXHP, maxHP));
			break;
		}
		case TRASH_MOB_ID: // ~1s fix (AoE-able)
			functions.push_back(RcStatFunction<StatSetFunction>::create(StatEnum::MAXHP, 1));
			functions.push_back(RcStatFunction<StatSetFunction>::create(StatEnum::SPEED, 6000 + rank * 100));
			break;
		case BULKY_MOB_ID: // ~10 ... 25s
			functions.push_back(RcStatFunction<StatSetFunction>::create(StatEnum::MAXHP, 10 * avgDps + rank * avgDps / 2));
			break;
		case DOMINATOR_MOB_ID: // ~10s fix
			functions.push_back(RcStatFunction<StatSetFunction>::create(StatEnum::MAXHP, 10 * avgDps));
			functions.push_back(RcStatFunction<StatSetFunction>::create(StatEnum::PHYSICAL_ATTACK, 0)); // only debuffs, no dmg
			functions.push_back(RcStatFunction<StatSetFunction>::create(StatEnum::ATTACK_SPEED, 2000 - rank * 30));
			break;
		default:
			break;
	}

	std::vector<runtime::Ptr<model::stats::calc::functions::IStatFunction>> borrowed(functions.begin(), functions.end());
	npc.getGameStats()->addEffect(nullptr, borrowed);
	npc.getLifeStats()->setCurrentHp(npc.getLifeStats()->getMaxHp());
}

// Java RoahCustomInstanceHandler.java:301-331
void RoahCustomInstanceHandler::setResult(bool success) {
	setResult(success, "");
}

// Java RoahCustomInstanceHandler.java:301-331, plus the C++-dev DPS report
void RoahCustomInstanceHandler::setResult(bool success, std::string_view failureReason) {
	cancelAllTasks();
	if (isCompleted.compareAndSet(false, true)) {
		int32_t oldRank = rank.get();
		runtime::Ptr<Player> player = instance->getPlayer(playerObjId.get());
		if (success) {
			rank.set(rank.get() + 1);
			PacketSendUtility::broadcastToMap(*instance,
				SM_MESSAGE(0, "", "Your rank has been increased to " + getRankDescription(rank.get())
					+ ". Stay steadfast for tougher challenges and higher rewards!", ChatType::BRIGHT_YELLOW_CENTER));
		} else {
			rank.set(std::max(0, rank.get() - 1)); // Java: Math.max(0, --rank)

			if (player != nullptr) {
				PacketSendUtility::sendPacket(*player, SM_MESSAGE(0, "",
					"Your rank has been decreased to " + getRankDescription(rank.get()) + ".", ChatType::BRIGHT_YELLOW_CENTER));

				if (rank.get() > 0) // prevent suicide abuse || loss rewards
					services::item::ItemService::addItem(*player, CustomInstanceService::REWARD_COIN_ID, getRewardCoinAmount(oldRank), true);
			}
		}

		if (success) { // C++-dev only: the run's DPS report in the player's chat log
			sendReport(dpsReport->successReport(getRankDescription(oldRank), getRankDescription(rank.get())));
		} else {
			closeRunDamage(nullptr);
			dpsReport->runEnded();
			sendReport(dpsReport->failureReport(failureReason));
		}

		CustomInstanceService::getInstance().saveNewPlayerModelEntries(playerObjId.get());
		if (CustomInstanceService::getInstance().changePlayerRank(playerObjId.get(), rank.get(), achievedDps.get())) {
			std::optional<std::string> name = player != nullptr ? std::optional<std::string>(player->getName()) : dao::PlayerDAO::getPlayerNameByObjId(playerObjId.get());
			log.info(std::format("[CI_ROAH] Rank changed for Player [id={}, name={}, oldRank={}({}), newRank={}({})]", playerObjId.get(),
				name.value_or("null"), getRankDescription(oldRank), oldRank, getRankDescription(rank.get()), rank.get()));
		}
	}
}

void RoahCustomInstanceHandler::sendReport(const std::string& text) {
	if (runtime::Ptr<Player> player = instance->getPlayer(playerObjId.get()))
		PacketSendUtility::sendMessage(*player, text); // GOLDEN_YELLOW: the chat log
}

int64_t RoahCustomInstanceHandler::playerDamageOn(Npc& npc) {
	int64_t damage = 0;
	for (const runtime::Ptr<controllers::attack::AggroInfo>& info : npc.getAggroList().stream()) {
		if (info->getDamage() <= 0)
			continue;
		// the player's summons and servants count as the player, as AggroList.getFinalDamageList attributes them to their master
		runtime::Ptr<model::gameobjects::Creature> master = info->getAttacker()->getMaster();
		if (master != nullptr && master->getObjectId() == playerObjId.get())
			damage += info->getDamage();
	}
	return damage;
}

void RoahCustomInstanceHandler::addDamageOnAliveNpcs(const Npc* except) {
	for (runtime::Ptr<Npc> n : instance->getNpcs()) {
		if (n.get() != except && !n->isDead())
			dpsReport->addDamage(playerDamageOn(*n));
	}
}

void RoahCustomInstanceHandler::closeRunDamage(const Npc* except) {
	if (runDamageClosed.compareAndSet(false, true))
		addDamageOnAliveNpcs(except);
}

// Java RoahCustomInstanceHandler.java:333-342
void RoahCustomInstanceHandler::cancelAllTasks() {
	if (runtime::Ptr<runtime::Future> task = despawnTask.get())
		task->cancel(true);
	if (runtime::Ptr<runtime::Future> task = trashMobSpawnTask.get())
		task->cancel(true);
	if (runtime::Ptr<runtime::Future> task = bulkyMobSpawnTask.get())
		task->cancel(true);
	if (runtime::Ptr<runtime::Future> task = dominatorMobSpawnTask.get())
		task->cancel(true);
}

// Java RoahCustomInstanceHandler.java:344-388
void RoahCustomInstanceHandler::spawnUnlockableNpcs(Player& player) {
	const int32_t currentRank = rank.get();
	if (currentRank >= getMinRank(CustomInstanceRankEnum::BRONZE)) { // + General Goods Merchant
		if (player.getRace() == model::Race::ASMODIANS)
			spawn(205848, 510.84058f, 407.1751f, 93.91156f, int8_t{87});
		else
			spawn(205826, 510.84058f, 407.1751f, 93.91156f, int8_t{87});
	}

	if (currentRank >= getMinRank(CustomInstanceRankEnum::SILVER)) // + Warehouse Manager
		spawn(205711, 496.6516f, 407.25f, 93.85523f, int8_t{99});

	if (currentRank >= getMinRank(CustomInstanceRankEnum::GOLD)) { // + Legion Warehouse
		if (player.getRace() == model::Race::ASMODIANS)
			spawn(805253, 494.753f, 405.53464f, 93.87636f, int8_t{104});
		else
			spawn(805245, 494.753f, 405.53464f, 93.87636f, int8_t{104});
	}

	if (currentRank >= getMinRank(CustomInstanceRankEnum::PLATINUM)) { // + Mail
		if (player.getRace() == model::Race::ASMODIANS)
			spawn(700079, 499.6871f, 406.73828f, 93.84634f, int8_t{91});
		else
			spawn(700000, 499.6871f, 406.73828f, 93.84634f, int8_t{91});
	}

	if (currentRank >= getMinRank(CustomInstanceRankEnum::MITHRIL)) // + Soul Healer
		spawn(205727, 513.6179f, 405.60223f, 93.91156f, int8_t{82});

	if (currentRank >= getMinRank(CustomInstanceRankEnum::CERANIUM)) { // + Trade Broker
		if (player.getRace() == model::Race::ASMODIANS)
			spawn(205853, 516.24396f, 402.30844f, 93.91156f, int8_t{68});
		else
			spawn(798912, 516.24396f, 402.30844f, 93.91156f, int8_t{68});
	}

	if (currentRank >= getMinRank(CustomInstanceRankEnum::ANCIENT)) { // + Stigma Master
		if (player.getRace() == model::Race::ASMODIANS)
			spawn(800765, 493.39554f, 403.7841f, 93.88781f, int8_t{106});
		else
			spawn(800760, 493.39554f, 403.7841f, 93.88781f, int8_t{106});
	}

	if (currentRank >= getMinRank(CustomInstanceRankEnum::ANCIENT_PLUS)) // + AP Shugo
		spawn(804833, 508.1798f, 407.87378f, 93.8523f, int8_t{89});
}

// Java RoahCustomInstanceHandler.java:390-392
int32_t RoahCustomInstanceHandler::getElapsedTimeMillis() {
	return static_cast<int32_t>(commons::utils::currentTimeMillis() - startTime.get());
}

// Java RoahCustomInstanceHandler.java:394-398
bool RoahCustomInstanceHandler::onReviveEvent(Player& player) {
	PacketSendUtility::sendPacket(player, SM_QUEST_ACTION(0, TIME_LIMIT * 1000 - getElapsedTimeMillis()));
	return GeneralInstanceHandler::onReviveEvent(player);
}

// Java RoahCustomInstanceHandler.java:400-414
bool RoahCustomInstanceHandler::onDie(Player& player, model::gameobjects::Creature& lastAttacker) {
	static_cast<void>(lastAttacker);
	if (!isCompleted.get()) {
		closeRunDamage(nullptr); // C++-dev only: before the npcs are deleted below
		if (isBossPhase.get()) {
			PacketSendUtility::sendMessage(player, "At last! I have become .. your greatest nightmare!", ChatType::BRIGHT_YELLOW_CENTER);
			deleteAliveNpcs({BOSS_MOB_A_M_ID, BOSS_MOB_A_F_ID, BOSS_MOB_E_M_ID, BOSS_MOB_E_F_ID, BOSS_MOB_AT_ID});
		} else {
			PacketSendUtility::sendMessage(player, "You shall not pass!", ChatType::BRIGHT_YELLOW_CENTER);
			deleteAliveNpcs({CENTER_ARTIFACT_ID, TRASH_MOB_ID, BULKY_MOB_ID, DOMINATOR_MOB_ID});
		}
		setResult(false, "you died"); // Java: setResult(false); C++-dev: the reason for the DPS report
	}
	return true;
}

// Java RoahCustomInstanceHandler.java:416-420
void RoahCustomInstanceHandler::onEndCastSkill(skillengine::model::Skill& skill) {
	if (isBossPhase.get()) {
		runtime::Ptr<Player> effector = runtime::as<Player>(skill.getEffector());
		if (effector != nullptr && instance->isRegistered(effector->getObjectId()))
			CustomInstanceService::getInstance().recordPlayerModelEntry(*effector, skill, effector->getTarget());
	}
}

// Java RoahCustomInstanceHandler.java:430-433
bool RoahCustomInstanceHandler::allowSelfReviveBySkill() {
	return false;
}

// Java RoahCustomInstanceHandler.java:435-438
bool RoahCustomInstanceHandler::allowSelfReviveByItem() {
	return false;
}

// Java RoahCustomInstanceHandler.java:440-443
bool RoahCustomInstanceHandler::allowInstanceRevive() {
	return false;
}

RoahCustomInstanceHandler::~RoahCustomInstanceHandler() = default;

} // namespace aion::gameserver::custom::instance
