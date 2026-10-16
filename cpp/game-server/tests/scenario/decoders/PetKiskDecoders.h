#pragma once

// The toy pet and kisk packets the M5j stage-2 gate compares the server against (m5j-plan.md §10.4 Z10/Z11, §18.3 CP5 H-21): SM_PET in the
// arms the gate meets (LOAD_PETS, ADOPT, SURRENDER, SPAWN, DISMISS, MOOD, FOOD, SPECIAL_FUNCTION) and SM_KISK_UPDATE. The ride packets of Z9
// are SM_EMOTION (CombatDecoders.h decodeEmotion) and SM_ITEM_USAGE_ANIMATION (ItemDecoders.h); the kisk's bind point is SM_BIND_POINT_INFO
// (TravelDecoders.h), its spawn SM_NPC_INFO (PacketDecoders.h) and the revive offer SM_DIE (CombatDecoders.h).
//
// **m5a-plan.md D9:** every layout below is written from the Java `writeImpl` (SM_PET.java, SM_KISK_UPDATE.java) and the model classes it
// calls (PetAction, PetFunctionType, PetDopingBag.MAX_ITEMS). Nothing here includes or mirrors a C++ serverpackets header. Every decode function
// consumes the body exactly and throws DecodeError otherwise, and Java's literal constants are verified, not skipped.

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "decoders/PacketDecoders.h" // BodyReader and DecodeError

namespace aion::gameserver::scenario::decoders {

/** PetAction ids (PetAction.java:10-24) */
constexpr uint16_t PET_ACTION_LOAD_PETS = 0;
constexpr uint16_t PET_ACTION_ADOPT = 1;
constexpr uint16_t PET_ACTION_SURRENDER = 2;
constexpr uint16_t PET_ACTION_SPAWN = 3;
constexpr uint16_t PET_ACTION_DISMISS = 4;
constexpr uint16_t PET_ACTION_FOOD = 9;
constexpr uint16_t PET_ACTION_RENAME = 10;
constexpr uint16_t PET_ACTION_MOOD = 12;
constexpr uint16_t PET_ACTION_SPECIAL_FUNCTION = 13;

/** PetFunctionType ids (PetFunctionType.java:7-15): the specialty ids and NONE, APPEARANCE's 1 */
constexpr uint8_t PET_FUNCTION_WAREHOUSE = 0;
constexpr uint8_t PET_FUNCTION_FOOD = 1;
constexpr uint8_t PET_FUNCTION_DOPING = 2;
constexpr uint8_t PET_FUNCTION_LOOT = 3;
constexpr uint8_t PET_FUNCTION_NONE = 6;
constexpr uint16_t PET_FUNCTION_APPEARANCE = 1;

/** one specialty of SM_PET's writePetData: writeC(id), writeC(length), the length bytes */
struct PetFunction {
	uint8_t id = 0;
	std::vector<uint8_t> data;
};

/** SM_PET.writePetData (SM_PET.java:268-316) */
struct PetData {
	std::string name;
	int32_t templateId = 0;
	int32_t objectId = 0;
	int32_t masterObjectId = 0;
	int32_t birthday = 0;
	/** secondsUntilExpiration(), 0 for a pet that does not expire */
	int32_t secondsUntilExpiration = 0;
	/** the specialties in their written order; the NONE fillers (writeH(NONE)) are not listed */
	std::vector<PetFunction> functions;
	/** writeAppearance: getDecoration() */
	int32_t decoration = 0;
};

/** SM_PET.writeImpl's SPAWN arm (SM_PET.java:176-191) */
struct PetSpawn {
	std::string name;
	int32_t templateId = 0;
	int32_t objectId = 0;
	float x = 0, y = 0, z = 0;
	float targetX = 0, targetY = 0, targetZ = 0;
	uint8_t heading = 0;
	int32_t masterObjectId = 0;
	int32_t decoration = 0;
};

struct Pet {
	uint16_t action = 0;
	/** LOAD_PETS */
	std::vector<PetData> pets;
	/** ADOPT */
	std::optional<PetData> adopted;
	/** SURRENDER: the template id and the object id */
	int32_t surrenderedTemplateId = 0;
	/** SURRENDER, DISMISS, RENAME: the pet's object id */
	int32_t petObjectId = 0;
	/** SPAWN */
	std::optional<PetSpawn> spawn;
	/** DISMISS: ObjectDeleteAnimation id */
	uint8_t animationId = 0;
	/** RENAME */
	std::string newName;
	/** FOOD, MOOD, SPECIAL_FUNCTION: the sub type */
	uint8_t subType = 0;
	/** FOOD: getFeedProgress().getDataForPacket(); MOOD 4: the mood points */
	int32_t value1 = 0;
	/** the remaining ints of the FOOD and MOOD arms, in their written order */
	std::vector<int32_t> values;
};

/** SM_PET.writeImpl (SM_PET.java:154-266): writeH(action) and the action's arm; TALK_WITH_MERCHANT etc. write nothing after the action */
Pet decodePet(std::span<const uint8_t> body);

/** SM_KISK_UPDATE.writeImpl (SM_KISK_UPDATE.java): eight ints */
struct KiskUpdate {
	int32_t kiskObjectId = 0;
	int32_t creatorId = 0;
	int32_t useMask = 0;
	int32_t currentMembers = 0;
	int32_t maxMembers = 0;
	int32_t remainingResurrects = 0;
	int32_t maxResurrects = 0;
	int32_t remainingLifetimeSeconds = 0;
};

KiskUpdate decodeKiskUpdate(std::span<const uint8_t> body);

} // namespace aion::gameserver::scenario::decoders
