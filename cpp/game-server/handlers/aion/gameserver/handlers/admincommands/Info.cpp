#include "aion/gameserver/handlers/admincommands/Info.h"

#include <typeinfo>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/ai/AbstractAI.h"
#include "aion/gameserver/ai/AISubState.h"
#include "aion/gameserver/controllers/attack/AggroInfo.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/NpcFactionsData.h"
#include "aion/gameserver/geoEngine/math/JavaFloat.h"
#include "aion/gameserver/model/CreatureType.h"
#include "aion/gameserver/model/TribeClass.h"
#include "aion/gameserver/model/gameobjects/Pet.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/gameobjects/player/npcFaction/NpcFaction.h"
#include "aion/gameserver/model/gameobjects/player/npcFaction/NpcFactions.h"
#include "aion/gameserver/model/gameobjects/siege/SiegeNpc.h"
#include "aion/gameserver/model/siege/FortressLocation.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/model/stats/container/PlayerLifeStats.h"
#include "aion/gameserver/model/templates/BoundRadius.h"
#include "aion/gameserver/model/templates/factions/NpcFactionTemplate.h"
#include "aion/gameserver/model/templates/npc/AbyssNpcType.h"
#include "aion/gameserver/model/templates/npc/NpcRank.h"
#include "aion/gameserver/model/templates/npc/NpcRating.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/npc/NpcTemplateType.h"
#include "aion/gameserver/model/templates/pet/PetTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/restrictions/PlayerRestrictions.h"
#include "aion/gameserver/services/SiegeService.h"
#include "aion/gameserver/services/TownService.h"
#include "aion/gameserver/services/panesterra/ahserion/PanesterraFaction.h"
#include "aion/gameserver/spawnengine/ClusteredNpc.h"
#include "aion/gameserver/spawnengine/WalkerGroup.h"
#include "aion/gameserver/spawnengine/WalkerGroupType.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/utils/SimpleClassName.h"
#include "aion/gameserver/utils/stats/StatFunctions.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(Info);

Info::Info()
	: AdminCommand("info", "Shows information about your target.",
		  " - Shows information about your target (defaults to your character, if no player is targeted).\n") {
}

std::string Info::floatStr(float value) {
	return geoEngine::math::JavaFloat::toString(value);
}

std::string Info::boolStr(bool value) {
	return value ? "true" : "false";
}

std::string Info::current(const std::unique_ptr<model::stats::calc::Stat2>& stat) {
	return std::to_string(stat->getCurrent());
}

// Java Info.java:40-161
void Info::execute(Player& admin, std::span<const std::string> /*params*/) {
	using xml::enumName;
	runtime::Ptr<VisibleObject> target = admin.getTarget() == nullptr ? runtime::Ptr<VisibleObject>(&admin) : admin.getTarget();

	sendInfo(admin, "[Info about " + utils::simpleClassName(typeid(*target)) + "]\n\tName: " + name(*target) + ", ID: " +
						std::to_string(target->getObjectTemplate()->getTemplateId()) + ", ObjectId: " + std::to_string(target->getObjectId()));
	if (runtime::Ptr<Creature> creature = runtime::as<Creature>(target)) {
		if (runtime::Ptr<Player> player = runtime::as<Player>(creature)) {
			runtime::Ptr<model::gameobjects::Pet> pet = player->getPet();
			sendInfo(admin, (pet != nullptr ? "\tPet: " + name(*pet) + ", ID: " + std::to_string(pet->getObjectTemplate()->getTemplateId()) +
												  ", ObjectId: " + std::to_string(pet->getObjectId()) + "\n"
											: std::string()) +
								"\tTown ID: " + std::to_string(TownService::getInstance().getTownResidence(*player)));
			for (int32_t i = 0; i < 2; i++) {
				runtime::Ptr<NpcFaction> faction = player->getNpcFactions().getActiveNpcFaction(i == 0);
				if (faction != nullptr) {
					sendInfo(admin, "\t" + std::string(i == 0 ? "Mentor" : "Daily") + " faction: " +
										DataManager::NPC_FACTIONS_DATA->getNpcFactionById(faction->getId())->getL10n() +
										", current quest state: " + std::string(enumName(faction->getState())) +
										(faction->getState() == ENpcFactionQuestState::COMPLETE
												? (", next after: " +
													  floatStr(static_cast<float>(faction->getTime() - commons::utils::currentTimeMillis() / 1000) / 3600.0f) + " h.")
												: std::string()));
				}
			}
			std::optional<services::panesterra::ahserion::PanesterraFaction> panesterraFaction = player->getPanesterraFaction();
			sendInfo(admin, "\tPanesterra faction: " + (panesterraFaction ? std::string(enumName(*panesterraFaction)) : std::string("null")));
			runtime::Ptr<PlayerGameStats> pgs = player->getGameStats();
			sendInfo(admin,
				"[Stats]"
				"\n\tHP: " + std::to_string(player->getLifeStats()->getCurrentHp()) + "/" + current(pgs->getMaxHp()) +
					", MP: " + std::to_string(player->getLifeStats()->getCurrentMp()) + "/" + current(pgs->getMaxMp()) +
					", FP: " + std::to_string(player->getLifeStats()->getCurrentFp()) + "/" + current(pgs->getFlyTime()) +
					", DP: " + std::to_string(player->getCommonData()->getDp()) + "/" + current(pgs->getMaxDp()) +
					"\n\tPower: " + current(pgs->getPower()) +
					", Health: " + current(pgs->getHealth()) +
					", Agility: " + current(pgs->getAgility()) +
					", Accuracy: " + current(pgs->getAccuracy()) +
					", Knowledge: " + current(pgs->getKnowledge()) +
					", Will: " + current(pgs->getWill()) +
					"\n\tCast Time Boost: " + floatStr(static_cast<float>(pgs->getStat(StatEnum::BOOST_CASTING_TIME, 1000)->getCurrent()) * 0.1f - 100) + "%" +
					"\n\tBase Attack Speed " + floatStr(static_cast<float>(pgs->getAttackSpeed()->getBase()) * 0.001f) +
					"\n\tCurrent Attack Speed: " + floatStr(static_cast<float>(pgs->getAttackSpeed()->getCurrent()) * 0.001f) +
					"\n\tMovement Speed: " + floatStr(pgs->getMovementSpeedFloat()) +
					"\n\t-------------Offence-------------" +
					"\n\tMagic Boost: " + current(pgs->getMBoost()) +
					"\n\tM. Accuracy: " + current(pgs->getMAccuracy()) +
					"\n\tM. Critical: " + current(pgs->getMCritical()) +
					"\n\t\t---------Main Hand-----------" +
					"\n\t\tM. Attack: " + current(pgs->getMainHandMAttack({CalculationType::DISPLAY})) +
					"\n\t\tP. Attack: " + current(pgs->getMainHandPAttack({CalculationType::DISPLAY})) +
					"\n\t\tP. Accuracy: " + current(pgs->getMainHandPAccuracy()) +
					"\n\t\tP. Critical: " + current(pgs->getMainHandPCritical()) +
					"\n\t\t-----------Off Hand-----------" +
					"\n\t\tM. Attack displayed: " + current(pgs->getOffHandMAttack({CalculationType::DISPLAY})) +
					", min: " + std::to_string(geoEngine::math::JavaFloat::doubleToInt(static_cast<float>(pgs->getOffHandMAttack()->getCurrent()) * pgs->getMinDamageRatio())) +
					", max: " + current(pgs->getOffHandMAttack()) +
					"\n\t\tP. Attack displayed: " + current(pgs->getOffHandPAttack({CalculationType::DISPLAY})) +
					", min: " + std::to_string(geoEngine::math::JavaFloat::doubleToInt(static_cast<float>(pgs->getOffHandPAttack()->getCurrent()) * pgs->getMinDamageRatio())) +
					", max: " + current(pgs->getOffHandPAttack()) +
					"\n\t\tP. Accuracy: " + current(pgs->getOffHandPAccuracy()) +
					"\n\t\tP. Critical: " + current(pgs->getOffHandPCritical()) +
					"\n\t-------------Defence--------------" +
					"\n\t\tM. Defence: " + current(pgs->getMDef()) +
					"\n\t\tMagic Resist: " + current(pgs->getMResist()) +
					"\n\t\tCrit. Spell Resist: " + pgs->getMCR()->toString() + // Java: the Stat2 itself is concatenated (Stat2.toString)
					"\n\t\tCrit. Spell Fortitude: " + current(pgs->getStat(StatEnum::MAGICAL_CRITICAL_DAMAGE_REDUCE, 0)) +
					"\n\t\tP. Defence: " + current(pgs->getPDef()) +
					"\n\t\tBlock: " + current(pgs->getBlock()) +
					"\n\t\tParry: " + current(pgs->getParry()) +
					"\n\t\tEvasion: " + current(pgs->getEvasion()) +
					"\n\t\tCrit. Strike Resist: " + current(pgs->getPCR()) +
					"\n\t\tCrit. Strike Fortitude: " + current(pgs->getStat(StatEnum::PHYSICAL_CRITICAL_DAMAGE_REDUCE, 0)) +
					"\n\t\tWind Defense: " + std::to_string(pgs->getElementalDefenseFor(SkillElement::WIND)) +
					"\n\t\tWater Defense: " + std::to_string(pgs->getElementalDefenseFor(SkillElement::WATER)) +
					"\n\t\tEarth Defense: " + std::to_string(pgs->getElementalDefenseFor(SkillElement::EARTH)) +
					"\n\t\tFire Defense: " + std::to_string(pgs->getElementalDefenseFor(SkillElement::FIRE)) +
					"\n\t\tDark Defense: " + std::to_string(pgs->getElementalDefenseFor(SkillElement::DARK)) +
					"\n\t\tLight Defense: " + std::to_string(pgs->getElementalDefenseFor(SkillElement::LIGHT)) +
					"\n\t-------------PvP Stats-------------" +
					"\n\tPvP attack: " + floatStr(static_cast<float>(pgs->getStat(StatEnum::PVP_ATTACK_RATIO, 0)->getCurrent()) * 0.1f) + "%" +
					"\n\tPvP p. attack: " + floatStr(static_cast<float>(pgs->getStat(StatEnum::PVP_ATTACK_RATIO_PHYSICAL, 0)->getCurrent()) * 0.1f) + "%" +
					"\n\tPvP m. attack: " + floatStr(static_cast<float>(pgs->getStat(StatEnum::PVP_ATTACK_RATIO_MAGICAL, 0)->getCurrent()) * 0.1f) + "%" +
					"\n\tPvP defend: " + floatStr(static_cast<float>(pgs->getStat(StatEnum::PVP_DEFEND_RATIO, 0)->getCurrent()) * 0.1f) + "%" +
					"\n\tPvP p. defend: " + floatStr(static_cast<float>(pgs->getStat(StatEnum::PVP_DEFEND_RATIO_PHYSICAL, 0)->getCurrent()) * 0.1f) + "%" +
					"\n\tPvP m. defend: " + floatStr(static_cast<float>(pgs->getStat(StatEnum::PVP_DEFEND_RATIO_MAGICAL, 0)->getCurrent()) * 0.1f) + "%");
		} else if (runtime::Ptr<Npc> npc = runtime::as<Npc>(creature)) {
			sendInfo(admin, "[Template info]\n\tRating: " + std::string(enumName(npc->getRating())) + ", Rank: " + std::string(enumName(npc->getRank())) +
								"\n\tTemplateType: " + std::string(enumName(npc->getNpcTemplateType())) +
								", AbyssType: " + std::string(enumName(npc->getAbyssNpcType())) +
								"\n\tRelative XP reward: " + std::to_string(StatFunctions::calculateExperienceReward(admin.getLevel(), *npc)));
			if (runtime::Ptr<SiegeNpc> siegeNpc = runtime::as<SiegeNpc>(npc))
				sendInfo(admin, "[Siege info]\n\tSiegeId: " + std::to_string(siegeNpc->getSiegeId()) +
									", SiegeRace: " + std::string(enumName(siegeNpc->getSiegeRace())));
			sendInfo(admin, "[AI info]\n\tAI: " + npc->getAi().getName() + "\n\tState: " + std::string(enumName(npc->getAi().getState())) +
								", SubState: " + std::string(enumName(npc->getAi().getSubState())));
			sendInfo(admin, "[Sense range]\n\tRadius: " + std::to_string(npc->getAggroRange()) +
								"\n\tShort-Radius: " + std::to_string(npc->getShortAggroRange()) +
								"\n\tAngle: " + std::to_string(npc->getAggroAngle()) +
								"\n\tSide: " + floatStr(npc->getObjectTemplate()->getBoundRadius()->getSide()) +
								", Front: " + floatStr(npc->getObjectTemplate()->getBoundRadius()->getFront()) +
								", Upper: " + floatStr(npc->getObjectTemplate()->getBoundRadius()->getUpper()) +
								"\n\tDirectional bound: " + floatStr(PositionUtil::getDirectionalBound(*npc, admin, true)) +
								"\n\tDistance: " + floatStr(static_cast<float>(npc->getAggroRange()) + PositionUtil::getDirectionalBound(*npc, admin, true)));
			sendInfo(admin, "[Spawn info]\n\tStaticId: " + std::to_string(npc->getSpawn()->getStaticId()) +
								", DistToSpawn: " + geoEngine::math::JavaFloat::doubleToString(npc->getDistanceToSpawnLocation()) + "m");
			if (npc->isPathWalker()) {
				std::optional<std::string> walkerId = npc->getSpawn()->getWalkerId();
				sendInfo(admin, "\tRouteId: " + walkerId.value_or("null"));
				if (npc->getWalkerGroup() != nullptr) {
					runtime::Ptr<ClusteredNpc> snpc = npc->getWalkerGroup()->getClusterData(*npc);
					std::optional<int32_t> walkerIndex = snpc->getWalkerIndex();
					sendInfo(admin, "\tWalkerGroupType: " + std::string(enumName(npc->getWalkerGroup()->getWalkType())) +
										", XDelta: " + floatStr(snpc->getXDelta()) + ", YDelta: " + floatStr(snpc->getYDelta()) +
										", Index: " + (walkerIndex ? std::to_string(*walkerIndex) : std::string("null")));
				}
			} else if (npc->isRandomWalker()) {
				sendInfo(admin, "\tRandomWalkRange: " + std::to_string(npc->getSpawn()->getRandomWalkRange()) + "m");
			}
		}
		sendInfo(admin, createZoneInfo(*creature));
		std::optional<model::TribeClass> tribe = creature->getTribe();
		sendInfo(admin, "[Tribe]\n\tRace: " + std::string(enumName(creature->getRace())) +
							", Tribe: " + (tribe ? std::string(enumName(*tribe)) : std::string("null")) +
							", TribeBase: " + std::string(enumName(creature->getBaseTribe())));
		sendInfo(admin, "[Your relation]\n\tisEnemy: " + boolStr(admin.isEnemy(*creature)) +
							", canAttack: " + boolStr(PlayerRestrictions::canAttack(admin, *target)));
		runtime::Ptr<Npc> npcTarget = runtime::as<Npc>(creature);
		sendInfo(admin, "[Targets relation]\n\tisEnemy: " + boolStr(creature->isEnemy(admin)) +
							(npcTarget != nullptr ? ", Hostility: " + std::string(enumName(npcTarget->getType(admin))) : std::string()));
		sendInfo(admin, "[Life stats]\n\tHP: " + std::to_string(creature->getLifeStats()->getCurrentHp()) + " / " +
							std::to_string(creature->getLifeStats()->getMaxHp()) + "\n\tMP: " + std::to_string(creature->getLifeStats()->getCurrentMp()) +
							" / " + std::to_string(creature->getLifeStats()->getMaxMp()));
		sendInfo(admin, createAggroInfo(*creature));
	} else if (target->getSpawn() != nullptr && target->getSpawn()->getStaticId() != 0) {
		sendInfo(admin, "\tStaticId: " + std::to_string(target->getSpawn()->getStaticId()));
	}
}

// Java Info.java:163-172
std::string Info::createZoneInfo(Creature& creature) {
	runtime::Ptr<FortressLocation> fortress = SiegeService::getInstance().findFortress(creature.getWorldId(), creature.getX(), creature.getY(), creature.getZ());
	int32_t townId = TownService::getInstance().getTownIdByPosition(creature);
	std::string sb = "[Current zone]";
	sb += "\n\t" + creature.getPosition()->toCoordString();
	sb += "\n\tFortress Location ID: " + (fortress == nullptr ? std::string("-") : std::to_string(fortress->getLocationId()));
	sb += "\n\tTown ID: " + (townId == 0 ? std::string("-") : std::to_string(townId));
	sb += "\n\tPvP: " + boolStr(creature.isInsidePvPZone());
	return sb;
}

// Java Info.java:174-195
std::string Info::createAggroInfo(Creature& creature) {
	std::string sb = "[AggroList]";
	int32_t aDmg = 0, eDmg = 0, tDmg = 0; // Java: AtomicInteger (only the lambda's captures needed them)
	for (const runtime::Ptr<controllers::attack::AggroInfo>& ai : creature.getAggroList().stream()) {
		runtime::Ptr<Creature> master = ai->getAttacker()->getMaster();
		std::string name = ChatCommand::name(*master);
		if (!master->equals(*ai->getAttacker()))
			name += "'s " + ChatCommand::name(*ai->getAttacker());
		tDmg += ai->getDamage();
		if (master->getRace() == Race::ASMODIANS)
			aDmg += ai->getDamage();
		else if (master->getRace() == Race::ELYOS)
			eDmg += ai->getDamage();
		sb += "\n\tName: " + name + ", Dmg: " + std::to_string(ai->getDamage()) + ", Hate: " + std::to_string(ai->getHate());
	}
	if (tDmg > 0) {
		sb += "\n\tTotal Dmg: " + std::to_string(tDmg);
		sb += "\n\t\t(A) Dmg: " + std::to_string(aDmg);
		sb += "\n\t\t(E) Dmg: " + std::to_string(eDmg);
		sb += "\n\t\t(N) Dmg: " + std::to_string(tDmg - aDmg - eDmg);
	}
	return sb;
}

} // namespace aion::gameserver::handlers::admincommands
