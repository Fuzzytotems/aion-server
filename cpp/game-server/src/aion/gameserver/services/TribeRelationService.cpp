#include "aion/gameserver/services/TribeRelationService.h"

#include <optional>
#include <string>

#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/TribeRelationsData.h"
#include "aion/gameserver/model/TribeClass.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/npc/AbyssNpcType.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/spawns/basespawns/BaseSpawnTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/panesterra/ahserion/PanesterraFaction.h"

namespace aion::gameserver::services {

namespace {

using model::TribeClass;
using model::gameobjects::Creature;
using model::templates::npc::AbyssNpcType;
using services::panesterra::ahserion::PanesterraFaction;
using Relation = bool (dataholders::TribeRelationsData::*)(TribeClass, TribeClass) const;

/** Java `switch (creature.getTribe())` and other dereferences of a null tribe throw NullPointerException */
TribeClass tribeOf(Creature& creature) {
	std::optional<TribeClass> tribe = creature.getTribe();
	if (!tribe)
		throw runtime::NullPointerException("Tribe of " + std::to_string(creature.getObjectId()) + " is null");
	return *tribe;
}

/** Java: DataManager.TRIBE_RELATIONS_DATA.isXRelation(tribe1, tribe2); a null tribe has no Tribe entry, so the relation is false */
bool relation(Relation isRelation, std::optional<TribeClass> tribe1, std::optional<TribeClass> tribe2) {
	if (!tribe1 || !tribe2)
		return false;
	return (*dataholders::DataManager::TRIBE_RELATIONS_DATA.*isRelation)(*tribe1, *tribe2);
}

/**
 * Java PanesterraFaction.getTribe() (the enum constructor argument). A local helper: the enum companion of PanesterraFaction (P5-12b) has no
 * C++ header yet.
 */
TribeClass tribeOf(PanesterraFaction faction) {
	switch (faction) {
		case PanesterraFaction::BALAUR:
			return TribeClass::GAB1_MONSTER;
		case PanesterraFaction::BELUS:
			return TribeClass::GAB1_01_POINT_01;
		case PanesterraFaction::IVY_TEMPLE:
			return TribeClass::GAB1_01_POINT_02;
		case PanesterraFaction::HIGHLAND_TEMPLE:
			return TribeClass::GAB1_01_POINT_03;
		case PanesterraFaction::ALPINE_TEMPLE:
			return TribeClass::GAB1_01_POINT_04;
		case PanesterraFaction::GRANDWEIR_TEMPLE:
			return TribeClass::GAB1_01_POINT_05;
		case PanesterraFaction::ASPIDA:
			return TribeClass::GAB1_02_POINT_01;
		case PanesterraFaction::NOERREN_TEMPLE:
			return TribeClass::GAB1_02_POINT_02;
		case PanesterraFaction::BOREALIS_TEMPLE:
			return TribeClass::GAB1_02_POINT_03;
		case PanesterraFaction::MYRKREN_TEMPLE:
			return TribeClass::GAB1_02_POINT_04;
		case PanesterraFaction::GLUMVEILEN_TEMPLE:
			return TribeClass::GAB1_02_POINT_05;
		case PanesterraFaction::ATANATOS:
			return TribeClass::GAB1_03_POINT_01;
		case PanesterraFaction::MEMORIA_TEMPLE:
			return TribeClass::GAB1_03_POINT_02;
		case PanesterraFaction::SYBILLINE_TEMPLE:
			return TribeClass::GAB1_03_POINT_03;
		case PanesterraFaction::AUSTERITY_TEMPLE:
			return TribeClass::GAB1_03_POINT_04;
		case PanesterraFaction::SERENITY_TEMPLE:
			return TribeClass::GAB1_03_POINT_05;
		case PanesterraFaction::DISILLON:
			return TribeClass::GAB1_04_POINT_01;
		case PanesterraFaction::NECROLUCE_TEMPLE:
			return TribeClass::GAB1_04_POINT_02;
		case PanesterraFaction::ESMERAUDUS_TEMPLE:
			return TribeClass::GAB1_04_POINT_03;
		case PanesterraFaction::VOLTAIC_TEMPLE:
			return TribeClass::GAB1_04_POINT_04;
		case PanesterraFaction::ILLUMINATUS_TEMPLE:
			return TribeClass::GAB1_04_POINT_05;
		case PanesterraFaction::PEACE:
			return TribeClass::GAB1_PEACE;
	}
	return TribeClass::GAB1_PEACE;
}

/**
 * Java: `creature2 instanceof Player p && p.getPanesterraFaction() != null && creature1.getTribe().name().startsWith("GAB1_")`; returns the
 * faction's tribe then.
 */
std::optional<TribeClass> panesterraTribe(Creature& creature1, Creature& creature2) {
	runtime::Ptr<model::gameobjects::player::Player> p = runtime::as<model::gameobjects::player::Player>(creature2);
	if (!p)
		return std::nullopt;
	std::optional<PanesterraFaction> faction = p->getPanesterraFaction();
	if (!faction || !xml::EnumTraits<TribeClass>::names[static_cast<size_t>(tribeOf(creature1))].starts_with("GAB1_"))
		return std::nullopt;
	return tribeOf(*faction);
}

} // namespace

bool TribeRelationService::isAggressive(model::gameobjects::Creature& creature1, model::gameobjects::Creature& creature2) {
	switch (tribeOf(creature1)) {
		case TribeClass::AGGRESSIVESINGLEMONSTER:
			if (creature2.getTribe() == TribeClass::YUN_GUARD)
				return true;
			break;
		case TribeClass::IDF5U2_SHULACK:
			if (creature2.getTribe() == TribeClass::FIELD_OBJECT_ALL_HOSTILEMONSTER)
				return false;
			break;
		default:
			break;
	}
	switch (creature1.getBaseTribe()) {
		case TribeClass::GUARD_DARK:
			switch (creature2.getBaseTribe()) {
				case TribeClass::PC:
				case TribeClass::GUARD:
				case TribeClass::GENERAL:
				case TribeClass::GUARD_DRAGON:
					return true;
				default:
					break;
			}
			break;
		case TribeClass::GUARD:
			switch (creature2.getBaseTribe()) {
				case TribeClass::PC_DARK:
				case TribeClass::GUARD_DARK:
				case TribeClass::GENERAL_DARK:
				case TribeClass::GUARD_DRAGON:
					return true;
				default:
					break;
			}
			break;
		case TribeClass::GUARD_DRAGON:
			switch (creature2.getBaseTribe()) {
				case TribeClass::PC_DARK:
				case TribeClass::PC:
				case TribeClass::GUARD:
				case TribeClass::GUARD_DARK:
				case TribeClass::GENERAL_DARK:
				case TribeClass::GENERAL:
					return true;
				default:
					break;
			}
			break;
		default:
			break;
	}
	if (std::optional<TribeClass> factionTribe = panesterraTribe(creature1, creature2)) {
		if (creature1.getTribe() == factionTribe)
			return false;
		return relation(&dataholders::TribeRelationsData::isAggressiveRelation, creature1.getTribe(), factionTribe);
	}

	return relation(&dataholders::TribeRelationsData::isAggressiveRelation, creature1.getTribe(), creature2.getTribe());
}

bool TribeRelationService::isFriend(model::gameobjects::Creature& creature1, model::gameobjects::Creature& creature2) {
	if (creature1.getTribe() == creature2.getTribe()) // OR BASE ????
		return true;
	if (creature1.getTribe() == TribeClass::IDF5U2_SHULACK && creature2.getTribe() == TribeClass::FIELD_OBJECT_ALL_HOSTILEMONSTER)
		return true;
	switch (creature1.getBaseTribe()) {
		case TribeClass::USEALL:
		case TribeClass::FIELD_OBJECT_ALL:
			return true;
		case TribeClass::GENERAL_DARK:
			if (creature1.getTribe() != TribeClass::DRAMA_EVE_NONPC_DARKA && creature1.getTribe() != TribeClass::DRAMA_EVE_NONPC_DARKB) {
				switch (creature2.getBaseTribe()) {
					case TribeClass::PC_DARK:
					case TribeClass::GUARD_DARK:
						return true;
					default:
						break;
				}
			}
			break;
		case TribeClass::GENERAL:
			if (creature1.getTribe() != TribeClass::DRAMA_EVE_NONPC_A && creature1.getTribe() != TribeClass::DRAMA_EVE_NONPC_B) {
				switch (creature2.getBaseTribe()) {
					case TribeClass::PC:
					case TribeClass::GUARD:
						return true;
					default:
						break;
				}
			}
			break;
		case TribeClass::FIELD_OBJECT_LIGHT:
			if (creature2.getBaseTribe() == TribeClass::PC)
				return true;
			break;
		case TribeClass::FIELD_OBJECT_DARK:
			if (creature2.getBaseTribe() == TribeClass::PC_DARK)
				return true;
			break;
		default:
			break;
	}
	if (std::optional<TribeClass> factionTribe = panesterraTribe(creature1, creature2)) {
		if (creature1.getTribe() == factionTribe)
			return true;
		return relation(&dataholders::TribeRelationsData::isFriendlyRelation, creature1.getTribe(), factionTribe);
	}

	return relation(&dataholders::TribeRelationsData::isFriendlyRelation, creature1.getTribe(), creature2.getTribe());
}

bool TribeRelationService::isSupport(model::gameobjects::Creature& creature1, model::gameobjects::Creature& creature2) {
	if (creature1.getTribe() == creature2.getTribe() || creature1.getBaseTribe() == creature2.getTribe() ||
		creature1.getTribe() == creature2.getBaseTribe() || creature1.getBaseTribe() == creature2.getBaseTribe()) {
		return true;
	}
	switch (creature1.getBaseTribe()) {
		case TribeClass::GUARD_DARK:
			if (creature2.getBaseTribe() == TribeClass::PC_DARK)
				return true;
			break;
		case TribeClass::GUARD:
			if (creature2.getBaseTribe() == TribeClass::PC)
				return true;
			break;
		default:
			break;
	}
	if (std::optional<TribeClass> factionTribe = panesterraTribe(creature1, creature2)) {
		if (creature1.getTribe() == factionTribe)
			return true;
		return relation(&dataholders::TribeRelationsData::isSupportRelation, creature1.getTribe(), factionTribe);
	}

	return relation(&dataholders::TribeRelationsData::isSupportRelation, creature1.getTribe(), creature2.getTribe());
}

bool TribeRelationService::isNone(model::gameobjects::Creature& creature1, model::gameobjects::Creature& creature2) {
	runtime::Ptr<model::gameobjects::Npc> npc1 = runtime::as<model::gameobjects::Npc>(creature1);
	if (relation(&dataholders::TribeRelationsData::isAggressiveRelation, creature1.getTribe(), creature2.getTribe()) ||
		(npc1 && checkSiegeRelation(*npc1, creature2)) ||
		relation(&dataholders::TribeRelationsData::isHostileRelation, creature1.getTribe(), creature2.getTribe()) ||
		relation(&dataholders::TribeRelationsData::isNeutralRelation, creature1.getTribe(), creature2.getTribe())) {
		return false;
	}
	switch (creature1.getBaseTribe()) {
		case TribeClass::GAB1_PEACE:
		case TribeClass::GENERAL_DRAGON:
			return true;
		case TribeClass::GENERAL:
		case TribeClass::FIELD_OBJECT_LIGHT:
			if (creature2.getBaseTribe() == TribeClass::PC_DARK)
				return true;

			break;
		case TribeClass::GENERAL_DARK:
		case TribeClass::FIELD_OBJECT_DARK:
			if (creature2.getBaseTribe() == TribeClass::PC)
				return true;
			break;
		default:
			break;
	}
	return relation(&dataholders::TribeRelationsData::isNoneRelation, creature1.getTribe(), creature2.getTribe());
}

bool TribeRelationService::isNeutral(model::gameobjects::Creature& creature1, model::gameobjects::Creature& creature2) {
	return relation(&dataholders::TribeRelationsData::isNeutralRelation, creature1.getTribe(), creature2.getTribe());
}

bool TribeRelationService::isHostile(model::gameobjects::Creature& creature1, model::gameobjects::Creature& creature2) {
	if (runtime::Ptr<model::gameobjects::Npc> npc1 = runtime::as<model::gameobjects::Npc>(creature1); npc1 && checkSiegeRelation(*npc1, creature2))
		return true;
	if (creature1.getTribe() == TribeClass::IDF5U2_SHULACK && creature2.getTribe() == TribeClass::FIELD_OBJECT_ALL_HOSTILEMONSTER)
		return false;
	if (creature1.getBaseTribe() == TribeClass::MONSTER) {
		switch (creature2.getBaseTribe()) {
			case TribeClass::PC_DARK:
			case TribeClass::PC:
				return true;
			default:
				break;
		}
	}

	if (std::optional<TribeClass> factionTribe = panesterraTribe(creature1, creature2)) {
		if (creature1.getTribe() == factionTribe)
			return false;
		return relation(&dataholders::TribeRelationsData::isHostileRelation, creature1.getTribe(), factionTribe);
	}

	return relation(&dataholders::TribeRelationsData::isHostileRelation, creature1.getTribe(), creature2.getTribe());
}

bool TribeRelationService::checkSiegeRelation(model::gameobjects::Npc& npc, model::gameobjects::Creature& creature) {
	AbyssNpcType abyssNpcType = npc.getObjectTemplate()->getAbyssNpcType();
	return ((abyssNpcType != AbyssNpcType::ARTIFACT && abyssNpcType != AbyssNpcType::NONE) ||
			   runtime::as<model::templates::spawns::basespawns::BaseSpawnTemplate>(npc.getSpawn())) &&
			((npc.getBaseTribe() == TribeClass::GENERAL && creature.getTribe() == TribeClass::PC_DARK) ||
				(npc.getBaseTribe() == TribeClass::GENERAL_DARK && creature.getTribe() == TribeClass::PC)) ||
		npc.getBaseTribe() == TribeClass::GENERAL_DRAGON && abyssNpcType != AbyssNpcType::ARTIFACT;
}

bool TribeRelationService::canHelpCreature(model::gameobjects::Creature& creature, model::gameobjects::Creature& creatureAskingForSupport) {
	return relation(&dataholders::TribeRelationsData::canSupport, creature.getTribe(), creatureAskingForSupport.getTribe());
}

} // namespace aion::gameserver::services
