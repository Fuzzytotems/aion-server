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

std::string Info::floatStr(float value) { // parity: a helper for Java's string conversion in a concatenation (docs/deviations/C1.md)
	return geoEngine::math::JavaFloat::toString(value); // parity: a helper for Java's string conversion in a concatenation (docs/deviations/C1.md)
}

std::string Info::boolStr(bool value) { // parity: a helper for Java's string conversion in a concatenation (docs/deviations/C1.md)
	return value ? "true" : "false"; // parity: a helper for Java's string conversion in a concatenation (docs/deviations/C1.md)
}

std::string Info::current(const std::unique_ptr<model::stats::calc::Stat2>& stat) { // parity: a helper for Java's string conversion in a concatenation (docs/deviations/C1.md)
	return std::to_string(stat->getCurrent()); // parity: a helper for Java's string conversion in a concatenation (docs/deviations/C1.md)
}

// Java Info.java:40-161
void Info::execute(Player& admin, std::span<const std::string> /*params*/) {
	using xml::enumName;
	runtime::Ptr<VisibleObject> target = admin.getTarget() == nullptr ? runtime::Ptr<VisibleObject>(&admin) : admin.getTarget(); // parity= VisibleObject target = admin.getTarget() == null ? admin : admin.getTarget();

	sendInfo(admin, "[Info about " + utils::simpleClassName(typeid(*target)) + "]\n\tName: " + name(*target) + ", ID: " + // parity= sendInfo(admin, "[Info about " + target.getClass().getSimpleName() + "]\n\tName: " + name(target) + ", ID: " + target.getObjectTemplate().getTemplateId() + ", ObjectId: " + target.getObjectId());
						std::to_string(target->getObjectTemplate()->getTemplateId()) + ", ObjectId: " + std::to_string(target->getObjectId())); // parity: (continued)
	if (runtime::Ptr<Creature> creature = runtime::as<Creature>(target)) { // parity= if (target instanceof Creature creature) {
		if (runtime::Ptr<Player> player = runtime::as<Player>(creature)) { // parity= if (creature instanceof Player player) {
			runtime::Ptr<model::gameobjects::Pet> pet = player->getPet();
			sendInfo(admin, (pet != nullptr ? "\tPet: " + name(*pet) + ", ID: " + std::to_string(pet->getObjectTemplate()->getTemplateId()) + // parity= sendInfo(admin, (pet != null ? "\tPet: " + name(pet) + ", ID: " + pet.getObjectTemplate().getTemplateId() + ", ObjectId: " + pet.getObjectId() + "\n" : "") + "\tTown ID: " + TownService.getInstance().getTownResidence(player));
												  ", ObjectId: " + std::to_string(pet->getObjectId()) + "\n" // parity: (continued)
											: std::string()) + // parity: (continued)
								"\tTown ID: " + std::to_string(TownService::getInstance().getTownResidence(*player))); // parity: (continued)
			for (int32_t i = 0; i < 2; i++) {
				runtime::Ptr<NpcFaction> faction = player->getNpcFactions().getActiveNpcFaction(i == 0);
				if (faction != nullptr) {
					sendInfo(admin, "\t" + std::string(i == 0 ? "Mentor" : "Daily") + " faction: " + // parity= sendInfo(admin, "\t" + (i == 0 ? "Mentor" : "Daily") + " faction: " + DataManager.NPC_FACTIONS_DATA.getNpcFactionById(faction.getId()).getL10n() + ", current quest state: " + faction.getState().name() + (faction.getState().equals(ENpcFactionQuestState.COMPLETE) ? ( ", next after: " + ((faction.getTime() - System.currentTimeMillis() / 1000) / 3600f) + " h.") : ""));
										DataManager::NPC_FACTIONS_DATA->getNpcFactionById(faction->getId())->getL10n() + // parity: (continued)
										", current quest state: " + std::string(enumName(faction->getState())) + // parity: (continued)
										(faction->getState() == ENpcFactionQuestState::COMPLETE // parity: (continued)
												? (", next after: " + // parity: (continued)
													  floatStr(static_cast<float>(faction->getTime() - commons::utils::currentTimeMillis() / 1000) / 3600.0f) + " h.") // parity: (continued)
												: std::string())); // parity: (continued)
				}
			}
			std::optional<services::panesterra::ahserion::PanesterraFaction> panesterraFaction = player->getPanesterraFaction(); // parity: the player.getPanesterraFaction() of the next line
			sendInfo(admin, "\tPanesterra faction: " + (panesterraFaction ? std::string(enumName(*panesterraFaction)) : std::string("null"))); // parity= sendInfo(admin, "\tPanesterra faction: " + player.getPanesterraFaction());
			runtime::Ptr<PlayerGameStats> pgs = player->getGameStats();
			sendInfo(admin, // parity= sendInfo(admin,
				"[Stats]" // parity= "[Stats]"
				"\n\tHP: " + std::to_string(player->getLifeStats()->getCurrentHp()) + "/" + current(pgs->getMaxHp()) + // parity= + "\n\tHP: " +  player.getLifeStats().getCurrentHp() +  "/" + pgs.getMaxHp().getCurrent()
					", MP: " + std::to_string(player->getLifeStats()->getCurrentMp()) + "/" + current(pgs->getMaxMp()) + // parity= + ", MP: " + player.getLifeStats().getCurrentMp() + "/" + pgs.getMaxMp().getCurrent()
					", FP: " + std::to_string(player->getLifeStats()->getCurrentFp()) + "/" + current(pgs->getFlyTime()) + // parity= + ", FP: " + player.getLifeStats().getCurrentFp() + "/" + pgs.getFlyTime().getCurrent()
					", DP: " + std::to_string(player->getCommonData()->getDp()) + "/" + current(pgs->getMaxDp()) + // parity= + ", DP: " + player.getCommonData().getDp() + "/" + pgs.getMaxDp().getCurrent()
					"\n\tPower: " + current(pgs->getPower()) + // parity= + "\n\tPower: " + pgs.getPower().getCurrent()
					", Health: " + current(pgs->getHealth()) + // parity= + ", Health: " + pgs.getHealth().getCurrent()
					", Agility: " + current(pgs->getAgility()) + // parity= + ", Agility: " + pgs.getAgility().getCurrent()
					", Accuracy: " + current(pgs->getAccuracy()) + // parity= + ", Accuracy: " + pgs.getAccuracy().getCurrent()
					", Knowledge: " + current(pgs->getKnowledge()) + // parity= + ", Knowledge: " + pgs.getKnowledge().getCurrent()
					", Will: " + current(pgs->getWill()) + // parity= + ", Will: " + pgs.getWill().getCurrent()
					"\n\tCast Time Boost: " + floatStr(static_cast<float>(pgs->getStat(StatEnum::BOOST_CASTING_TIME, 1000)->getCurrent()) * 0.1f - 100) + "%" + // parity= + "\n\tCast Time Boost: " + (pgs.getStat(StatEnum.BOOST_CASTING_TIME, 1000).getCurrent() * 0.1f - 100) + "%"
					"\n\tBase Attack Speed " + floatStr(static_cast<float>(pgs->getAttackSpeed()->getBase()) * 0.001f) + // parity= + "\n\tBase Attack Speed " + pgs.getAttackSpeed().getBase() * 0.001f
					"\n\tCurrent Attack Speed: " + floatStr(static_cast<float>(pgs->getAttackSpeed()->getCurrent()) * 0.001f) + // parity= + "\n\tCurrent Attack Speed: " + pgs.getAttackSpeed().getCurrent() * 0.001f
					"\n\tMovement Speed: " + floatStr(pgs->getMovementSpeedFloat()) + // parity= + "\n\tMovement Speed: " + pgs.getMovementSpeedFloat()
					"\n\t-------------Offence-------------" + // parity= + "\n\t-------------Offence-------------"
					"\n\tMagic Boost: " + current(pgs->getMBoost()) + // parity= + "\n\tMagic Boost: " + pgs.getMBoost().getCurrent()
					"\n\tM. Accuracy: " + current(pgs->getMAccuracy()) + // parity= + "\n\tM. Accuracy: " + pgs.getMAccuracy().getCurrent()
					"\n\tM. Critical: " + current(pgs->getMCritical()) + // parity= + "\n\tM. Critical: " + pgs.getMCritical().getCurrent()
					"\n\t\t---------Main Hand-----------" + // parity= + "\n\t\t---------Main Hand-----------"
					"\n\t\tM. Attack: " + current(pgs->getMainHandMAttack({CalculationType::DISPLAY})) + // parity= + "\n\t\tM. Attack: " + (pgs.getMainHandMAttack(CalculationType.DISPLAY).getCurrent())
					"\n\t\tP. Attack: " + current(pgs->getMainHandPAttack({CalculationType::DISPLAY})) + // parity= + "\n\t\tP. Attack: " + pgs.getMainHandPAttack(CalculationType.DISPLAY).getCurrent()
					"\n\t\tP. Accuracy: " + current(pgs->getMainHandPAccuracy()) + // parity= + "\n\t\tP. Accuracy: " + pgs.getMainHandPAccuracy().getCurrent()
					"\n\t\tP. Critical: " + current(pgs->getMainHandPCritical()) + // parity= + "\n\t\tP. Critical: " + pgs.getMainHandPCritical().getCurrent()
					"\n\t\t-----------Off Hand-----------" + // parity= + "\n\t\t-----------Off Hand-----------"
					"\n\t\tM. Attack displayed: " + current(pgs->getOffHandMAttack({CalculationType::DISPLAY})) + // parity= + "\n\t\tM. Attack displayed: " + (pgs.getOffHandMAttack(CalculationType.DISPLAY).getCurrent())
					", min: " + std::to_string(geoEngine::math::JavaFloat::doubleToInt(static_cast<float>(pgs->getOffHandMAttack()->getCurrent()) * pgs->getMinDamageRatio())) + // parity= + ", min: " + (int) (pgs.getOffHandMAttack().getCurrent() * pgs.getMinDamageRatio())
					", max: " + current(pgs->getOffHandMAttack()) + // parity= + ", max: " + pgs.getOffHandMAttack().getCurrent()
					"\n\t\tP. Attack displayed: " + current(pgs->getOffHandPAttack({CalculationType::DISPLAY})) + // parity= + "\n\t\tP. Attack displayed: " + (pgs.getOffHandPAttack(CalculationType.DISPLAY).getCurrent())
					", min: " + std::to_string(geoEngine::math::JavaFloat::doubleToInt(static_cast<float>(pgs->getOffHandPAttack()->getCurrent()) * pgs->getMinDamageRatio())) + // parity= + ", min: " + (int) (pgs.getOffHandPAttack().getCurrent() * pgs.getMinDamageRatio())
					", max: " + current(pgs->getOffHandPAttack()) + // parity= + ", max: " + pgs.getOffHandPAttack().getCurrent()
					"\n\t\tP. Accuracy: " + current(pgs->getOffHandPAccuracy()) + // parity= + "\n\t\tP. Accuracy: " + pgs.getOffHandPAccuracy().getCurrent()
					"\n\t\tP. Critical: " + current(pgs->getOffHandPCritical()) + // parity= + "\n\t\tP. Critical: " + pgs.getOffHandPCritical().getCurrent()
					"\n\t-------------Defence--------------" + // parity= + "\n\t-------------Defence--------------"
					"\n\t\tM. Defence: " + current(pgs->getMDef()) + // parity= + "\n\t\tM. Defence: " + pgs.getMDef().getCurrent()
					"\n\t\tMagic Resist: " + current(pgs->getMResist()) + // parity= + "\n\t\tMagic Resist: " + pgs.getMResist().getCurrent()
					"\n\t\tCrit. Spell Resist: " + pgs->getMCR()->toString() + // parity= + "\n\t\tCrit. Spell Resist: " + pgs.getMCR() // Java concatenates the Stat2 itself (Stat2.toString)
					"\n\t\tCrit. Spell Fortitude: " + current(pgs->getStat(StatEnum::MAGICAL_CRITICAL_DAMAGE_REDUCE, 0)) + // parity= + "\n\t\tCrit. Spell Fortitude: " + pgs.getStat(StatEnum.MAGICAL_CRITICAL_DAMAGE_REDUCE, 0).getCurrent()
					"\n\t\tP. Defence: " + current(pgs->getPDef()) + // parity= + "\n\t\tP. Defence: " + pgs.getPDef().getCurrent()
					"\n\t\tBlock: " + current(pgs->getBlock()) + // parity= + "\n\t\tBlock: " + pgs.getBlock().getCurrent()
					"\n\t\tParry: " + current(pgs->getParry()) + // parity= + "\n\t\tParry: " + pgs.getParry().getCurrent()
					"\n\t\tEvasion: " + current(pgs->getEvasion()) + // parity= + "\n\t\tEvasion: " + pgs.getEvasion().getCurrent()
					"\n\t\tCrit. Strike Resist: " + current(pgs->getPCR()) + // parity= + "\n\t\tCrit. Strike Resist: " + pgs.getPCR().getCurrent()
					"\n\t\tCrit. Strike Fortitude: " + current(pgs->getStat(StatEnum::PHYSICAL_CRITICAL_DAMAGE_REDUCE, 0)) + // parity= + "\n\t\tCrit. Strike Fortitude: " + pgs.getStat(StatEnum.PHYSICAL_CRITICAL_DAMAGE_REDUCE, 0).getCurrent()
					"\n\t\tWind Defense: " + std::to_string(pgs->getElementalDefenseFor(SkillElement::WIND)) + // parity= + "\n\t\tWind Defense: " + pgs.getElementalDefenseFor(SkillElement.WIND)
					"\n\t\tWater Defense: " + std::to_string(pgs->getElementalDefenseFor(SkillElement::WATER)) + // parity= + "\n\t\tWater Defense: " + pgs.getElementalDefenseFor(SkillElement.WATER)
					"\n\t\tEarth Defense: " + std::to_string(pgs->getElementalDefenseFor(SkillElement::EARTH)) + // parity= + "\n\t\tEarth Defense: " + pgs.getElementalDefenseFor(SkillElement.EARTH)
					"\n\t\tFire Defense: " + std::to_string(pgs->getElementalDefenseFor(SkillElement::FIRE)) + // parity= + "\n\t\tFire Defense: " + pgs.getElementalDefenseFor(SkillElement.FIRE)
					"\n\t\tDark Defense: " + std::to_string(pgs->getElementalDefenseFor(SkillElement::DARK)) + // parity= + "\n\t\tDark Defense: " + pgs.getElementalDefenseFor(SkillElement.DARK)
					"\n\t\tLight Defense: " + std::to_string(pgs->getElementalDefenseFor(SkillElement::LIGHT)) + // parity= + "\n\t\tLight Defense: " + pgs.getElementalDefenseFor(SkillElement.LIGHT)
					"\n\t-------------PvP Stats-------------" + // parity= + "\n\t-------------PvP Stats-------------"
					"\n\tPvP attack: " + floatStr(static_cast<float>(pgs->getStat(StatEnum::PVP_ATTACK_RATIO, 0)->getCurrent()) * 0.1f) + "%" + // parity= + "\n\tPvP attack: " + pgs.getStat(StatEnum.PVP_ATTACK_RATIO, 0).getCurrent() * 0.1f + "%"
					"\n\tPvP p. attack: " + floatStr(static_cast<float>(pgs->getStat(StatEnum::PVP_ATTACK_RATIO_PHYSICAL, 0)->getCurrent()) * 0.1f) + "%" + // parity= + "\n\tPvP p. attack: " + pgs.getStat(StatEnum.PVP_ATTACK_RATIO_PHYSICAL, 0).getCurrent() * 0.1f + "%"
					"\n\tPvP m. attack: " + floatStr(static_cast<float>(pgs->getStat(StatEnum::PVP_ATTACK_RATIO_MAGICAL, 0)->getCurrent()) * 0.1f) + "%" + // parity= + "\n\tPvP m. attack: " + pgs.getStat(StatEnum.PVP_ATTACK_RATIO_MAGICAL, 0).getCurrent() * 0.1f + "%"
					"\n\tPvP defend: " + floatStr(static_cast<float>(pgs->getStat(StatEnum::PVP_DEFEND_RATIO, 0)->getCurrent()) * 0.1f) + "%" + // parity= + "\n\tPvP defend: " + pgs.getStat(StatEnum.PVP_DEFEND_RATIO, 0).getCurrent() * 0.1f + "%"
					"\n\tPvP p. defend: " + floatStr(static_cast<float>(pgs->getStat(StatEnum::PVP_DEFEND_RATIO_PHYSICAL, 0)->getCurrent()) * 0.1f) + "%" + // parity= + "\n\tPvP p. defend: " + pgs.getStat(StatEnum.PVP_DEFEND_RATIO_PHYSICAL, 0).getCurrent() * 0.1f + "%"
					"\n\tPvP m. defend: " + floatStr(static_cast<float>(pgs->getStat(StatEnum::PVP_DEFEND_RATIO_MAGICAL, 0)->getCurrent()) * 0.1f) + "%"); // parity= + "\n\tPvP m. defend: " + pgs.getStat(StatEnum.PVP_DEFEND_RATIO_MAGICAL, 0).getCurrent() * 0.1f + "%");
		} else if (runtime::Ptr<Npc> npc = runtime::as<Npc>(creature)) { // parity= } else if (creature instanceof Npc npc) {
			sendInfo(admin, "[Template info]\n\tRating: " + std::string(enumName(npc->getRating())) + ", Rank: " + std::string(enumName(npc->getRank())) + // parity= sendInfo(admin, "[Template info]\n\tRating: " + npc.getRating() + ", Rank: " + npc.getRank() + "\n\tTemplateType: " + npc.getNpcTemplateType() + ", AbyssType: " + npc.getAbyssNpcType() + "\n\tRelative XP reward: " + StatFunctions.calculateExperienceReward(admin.getLevel(), npc));
								"\n\tTemplateType: " + std::string(enumName(npc->getNpcTemplateType())) + // parity: (continued)
								", AbyssType: " + std::string(enumName(npc->getAbyssNpcType())) + // parity: (continued)
								"\n\tRelative XP reward: " + std::to_string(StatFunctions::calculateExperienceReward(admin.getLevel(), *npc))); // parity: (continued)
			if (runtime::Ptr<SiegeNpc> siegeNpc = runtime::as<SiegeNpc>(npc)) // parity= if (npc instanceof SiegeNpc siegeNpc)
				sendInfo(admin, "[Siege info]\n\tSiegeId: " + std::to_string(siegeNpc->getSiegeId()) + // parity= sendInfo(admin, "[Siege info]\n\tSiegeId: " + siegeNpc.getSiegeId() + ", SiegeRace: " + siegeNpc.getSiegeRace());
									", SiegeRace: " + std::string(enumName(siegeNpc->getSiegeRace()))); // parity: (continued)
			sendInfo(admin, "[AI info]\n\tAI: " + npc->getAi().getName() + "\n\tState: " + std::string(enumName(npc->getAi().getState())) + // parity= sendInfo(admin, "[AI info]\n\tAI: " + npc.getAi().getName() + "\n\tState: " + npc.getAi().getState() + ", SubState: " + npc.getAi().getSubState());
								", SubState: " + std::string(enumName(npc->getAi().getSubState()))); // parity: (continued)
			sendInfo(admin, "[Sense range]\n\tRadius: " + std::to_string(npc->getAggroRange()) + // parity= sendInfo(admin, "[Sense range]\n\tRadius: " + npc.getAggroRange()
								"\n\tShort-Radius: " + std::to_string(npc->getShortAggroRange()) + // parity= + "\n\tShort-Radius: " + npc.getShortAggroRange()
								"\n\tAngle: " + std::to_string(npc->getAggroAngle()) + // parity= + "\n\tAngle: " + npc.getAggroAngle()
								"\n\tSide: " + floatStr(npc->getObjectTemplate()->getBoundRadius()->getSide()) + // parity= + "\n\tSide: " + npc.getObjectTemplate().getBoundRadius().getSide() + ", Front: " + npc.getObjectTemplate().getBoundRadius().getFront() + ", Upper: " + npc.getObjectTemplate().getBoundRadius().getUpper()
								", Front: " + floatStr(npc->getObjectTemplate()->getBoundRadius()->getFront()) + // parity: (continued)
								", Upper: " + floatStr(npc->getObjectTemplate()->getBoundRadius()->getUpper()) + // parity: (continued)
								"\n\tDirectional bound: " + floatStr(PositionUtil::getDirectionalBound(*npc, admin, true)) + // parity= + "\n\tDirectional bound: " + PositionUtil.getDirectionalBound(npc, admin, true)
								"\n\tDistance: " + floatStr(static_cast<float>(npc->getAggroRange()) + PositionUtil::getDirectionalBound(*npc, admin, true))); // parity= + "\n\tDistance: " + (npc.getAggroRange() + PositionUtil.getDirectionalBound(npc, admin, true)));
			sendInfo(admin, "[Spawn info]\n\tStaticId: " + std::to_string(npc->getSpawn()->getStaticId()) + // parity= sendInfo(admin, "[Spawn info]\n\tStaticId: " + npc.getSpawn().getStaticId() + ", DistToSpawn: " + npc.getDistanceToSpawnLocation() + "m");
								", DistToSpawn: " + geoEngine::math::JavaFloat::doubleToString(npc->getDistanceToSpawnLocation()) + "m"); // parity: (continued)
			if (npc->isPathWalker()) {
				std::optional<std::string> walkerId = npc->getSpawn()->getWalkerId(); // parity: the npc.getSpawn().getWalkerId() of the next line
				sendInfo(admin, "\tRouteId: " + walkerId.value_or("null")); // parity= sendInfo(admin, "\tRouteId: " + npc.getSpawn().getWalkerId());
				if (npc->getWalkerGroup() != nullptr) {
					runtime::Ptr<ClusteredNpc> snpc = npc->getWalkerGroup()->getClusterData(*npc);
					std::optional<int32_t> walkerIndex = snpc->getWalkerIndex(); // parity: the snpc.getWalkerIndex() of the next statement
					sendInfo(admin, "\tWalkerGroupType: " + std::string(enumName(npc->getWalkerGroup()->getWalkType())) + // parity= sendInfo(admin, "\tWalkerGroupType: " + npc.getWalkerGroup().getWalkType() + ", XDelta: " + snpc.getXDelta() + ", YDelta: " + snpc.getYDelta() + ", Index: " + snpc.getWalkerIndex());
										", XDelta: " + floatStr(snpc->getXDelta()) + ", YDelta: " + floatStr(snpc->getYDelta()) + // parity: (continued)
										", Index: " + (walkerIndex ? std::to_string(*walkerIndex) : std::string("null"))); // parity: (continued)
				}
			} else if (npc->isRandomWalker()) {
				sendInfo(admin, "\tRandomWalkRange: " + std::to_string(npc->getSpawn()->getRandomWalkRange()) + "m");
			}
		}
		sendInfo(admin, createZoneInfo(*creature));
		std::optional<model::TribeClass> tribe = creature->getTribe(); // parity: the creature.getTribe() of the next statement
		sendInfo(admin, "[Tribe]\n\tRace: " + std::string(enumName(creature->getRace())) + // parity= sendInfo(admin, "[Tribe]\n\tRace: " + creature.getRace() + ", Tribe: " + creature.getTribe() + ", TribeBase: " + creature.getBaseTribe());
							", Tribe: " + (tribe ? std::string(enumName(*tribe)) : std::string("null")) + // parity: (continued)
							", TribeBase: " + std::string(enumName(creature->getBaseTribe()))); // parity: (continued)
		sendInfo(admin, "[Your relation]\n\tisEnemy: " + boolStr(admin.isEnemy(*creature)) + // parity= sendInfo(admin, "[Your relation]\n\tisEnemy: " + admin.isEnemy(creature) + ", canAttack: " + PlayerRestrictions.canAttack(admin, target));
							", canAttack: " + boolStr(PlayerRestrictions::canAttack(admin, *target))); // parity: (continued)
		runtime::Ptr<Npc> npcTarget = runtime::as<Npc>(creature);
		sendInfo(admin, "[Targets relation]\n\tisEnemy: " + boolStr(creature->isEnemy(admin)) + // parity= sendInfo(admin, "[Targets relation]\n\tisEnemy: " + creature.isEnemy(admin) + (creature instanceof Npc ? ", Hostility: " + ((Npc) creature).getType(admin) : ""));
							(npcTarget != nullptr ? ", Hostility: " + std::string(enumName(npcTarget->getType(admin))) : std::string())); // parity: (continued)
		sendInfo(admin, "[Life stats]\n\tHP: " + std::to_string(creature->getLifeStats()->getCurrentHp()) + " / " + // parity= sendInfo(admin, "[Life stats]\n\tHP: " + creature.getLifeStats().getCurrentHp() + " / " + creature.getLifeStats().getMaxHp() + "\n\tMP: " + creature.getLifeStats().getCurrentMp() + " / " + creature.getLifeStats().getMaxMp());
							std::to_string(creature->getLifeStats()->getMaxHp()) + "\n\tMP: " + std::to_string(creature->getLifeStats()->getCurrentMp()) + // parity: (continued)
							" / " + std::to_string(creature->getLifeStats()->getMaxMp())); // parity: (continued)
		sendInfo(admin, createAggroInfo(*creature));
	} else if (target->getSpawn() != nullptr && target->getSpawn()->getStaticId() != 0) {
		sendInfo(admin, "\tStaticId: " + std::to_string(target->getSpawn()->getStaticId()));
	}
}

// Java Info.java:163-172
std::string Info::createZoneInfo(Creature& creature) {
	runtime::Ptr<FortressLocation> fortress = SiegeService::getInstance().findFortress(creature.getWorldId(), creature.getX(), creature.getY(), creature.getZ());
	int32_t townId = TownService::getInstance().getTownIdByPosition(creature);
	std::string sb = "[Current zone]"; // parity= StringBuilder sb = new StringBuilder("[Current zone]");
	sb += "\n\t" + creature.getPosition()->toCoordString(); // parity= sb.append("\n\t" + creature.getPosition().toCoordString());
	sb += "\n\tFortress Location ID: " + (fortress == nullptr ? std::string("-") : std::to_string(fortress->getLocationId())); // parity= sb.append("\n\tFortress Location ID: " + (fortress == null ? "-" : fortress.getLocationId()));
	sb += "\n\tTown ID: " + (townId == 0 ? std::string("-") : std::to_string(townId)); // parity= sb.append("\n\tTown ID: " + (townId == 0 ? "-" : townId));
	sb += "\n\tPvP: " + boolStr(creature.isInsidePvPZone()); // parity= sb.append("\n\tPvP: " + creature.isInsidePvPZone());
	return sb; // parity= return sb.toString();
}

// Java Info.java:174-195
std::string Info::createAggroInfo(Creature& creature) {
	std::string sb = "[AggroList]"; // parity= StringBuilder sb = new StringBuilder("[AggroList]");
	int32_t aDmg = 0, eDmg = 0, tDmg = 0; // parity= AtomicInteger aDmg = new AtomicInteger(), eDmg = new AtomicInteger(), tDmg = new AtomicInteger(); // Java: AtomicInteger (only the lambda's captures needed them)
	for (const runtime::Ptr<controllers::attack::AggroInfo>& ai : creature.getAggroList().stream()) { // parity= creature.getAggroList().stream().forEach(ai -> {
		runtime::Ptr<Creature> master = ai->getAttacker()->getMaster();
		std::string name = ChatCommand::name(*master);
		if (!master->equals(*ai->getAttacker()))
			name += "'s " + ChatCommand::name(*ai->getAttacker());
		tDmg += ai->getDamage(); // parity= tDmg.addAndGet(ai.getDamage());
		if (master->getRace() == Race::ASMODIANS)
			aDmg += ai->getDamage(); // parity= aDmg.addAndGet(ai.getDamage());
		else if (master->getRace() == Race::ELYOS)
			eDmg += ai->getDamage(); // parity= eDmg.addAndGet(ai.getDamage());
		sb += "\n\tName: " + name + ", Dmg: " + std::to_string(ai->getDamage()) + ", Hate: " + std::to_string(ai->getHate()); // parity= sb.append("\n\tName: " + name + ", Dmg: " + ai.getDamage() + ", Hate: " + ai.getHate());
	}
	if (tDmg > 0) { // parity= if (tDmg.get() > 0) {
		sb += "\n\tTotal Dmg: " + std::to_string(tDmg); // parity= sb.append("\n\tTotal Dmg: ").append(tDmg);
		sb += "\n\t\t(A) Dmg: " + std::to_string(aDmg); // parity= sb.append("\n\t\t(A) Dmg: ").append(aDmg);
		sb += "\n\t\t(E) Dmg: " + std::to_string(eDmg); // parity= sb.append("\n\t\t(E) Dmg: ").append(eDmg);
		sb += "\n\t\t(N) Dmg: " + std::to_string(tDmg - aDmg - eDmg); // parity= sb.append("\n\t\t(N) Dmg: ").append(tDmg.get() - aDmg.get() - eDmg.get());
	}
	return sb; // parity= return sb.toString();
}

} // namespace aion::gameserver::handlers::admincommands
