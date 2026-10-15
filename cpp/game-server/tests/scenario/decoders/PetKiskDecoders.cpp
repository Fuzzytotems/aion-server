#include "decoders/PetKiskDecoders.h"

#include <string>
#include <utility>

namespace aion::gameserver::scenario::decoders {

namespace {

/** PetDopingBag.MAX_ITEMS (PetDopingBag.java:10): the DOPING specialty always writes this many ints */
constexpr size_t DOPING_MAX_ITEMS = 8;

/** SM_PET.writeAppearance (SM_PET.java:318-325): writeH(APPEARANCE), three zero colour bytes, the decoration, two zero ints */
int32_t readAppearance(BodyReader& reader) {
	reader.expectH(PET_FUNCTION_APPEARANCE, "SM_PET writeH(PetFunctionType.APPEARANCE.getId())");
	reader.expectZeros(3, "SM_PET appearance colours (not implemented: writeC(0) x 3)");
	const int32_t decoration = reader.D();
	reader.expectZeros(8, "SM_PET appearance writeD(0) x 2");
	return decoration;
}

/** SM_PET.writePetData (SM_PET.java:268-316) */
PetData readPetData(BodyReader& reader) {
	PetData data;
	data.name = reader.S();
	data.templateId = reader.D();
	data.objectId = reader.D();
	data.masterObjectId = reader.D();
	reader.expectZeros(8, "SM_PET writePetData writeD(0) x 2");
	data.birthday = reader.D();
	data.secondsUntilExpiration = reader.D();
	// the specialties (pets have two at most, :311 "Pets have only 2 functions max. If absent filled with NONE"); a NONE filler is writeH(NONE),
	// which reads as id NONE and length 0
	for (int slot = 0; slot < 2; slot++) {
		PetFunction function;
		function.id = reader.C();
		const uint8_t length = reader.C();
		if (function.id == PET_FUNCTION_NONE) {
			if (length != 0)
				reader.fail("SM_PET writeH(PetFunctionType.NONE.getId()) has a second byte " + std::to_string(length));
			continue;
		}
		if (function.id == PET_FUNCTION_DOPING && length != DOPING_MAX_ITEMS * 4)
			reader.fail("SM_PET DOPING writeC(PetDopingBag.MAX_ITEMS * 4) is " + std::to_string(length));
		if ((function.id == PET_FUNCTION_FOOD && length != 8) || (function.id == PET_FUNCTION_LOOT && length != 1) ||
			(function.id == PET_FUNCTION_WAREHOUSE && length != 0))
			reader.fail("SM_PET specialty " + std::to_string(function.id) + " with length " + std::to_string(length));
		function.data = reader.B(length);
		data.functions.push_back(std::move(function));
	}
	data.decoration = readAppearance(reader);
	return data;
}

} // namespace

Pet decodePet(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_PET");
	Pet pet;
	pet.action = reader.H(); // writeH(action.getActionId())
	switch (pet.action) {
		case PET_ACTION_LOAD_PETS: { // :156-162
			reader.expectC(0, "SM_PET LOAD_PETS writeC(0)");
			const uint16_t count = reader.H();
			for (uint16_t i = 0; i < count; i++)
				pet.pets.push_back(readPetData(reader));
			break;
		}
		case PET_ACTION_ADOPT: // :163-165
			pet.adopted = readPetData(reader);
			break;
		case PET_ACTION_SURRENDER: // :166-171
			pet.surrenderedTemplateId = reader.D();
			pet.petObjectId = reader.D();
			reader.expectZeros(8, "SM_PET SURRENDER writeD(0) x 2");
			break;
		case PET_ACTION_SPAWN: { // :172-187
			PetSpawn spawn;
			spawn.name = reader.S();
			spawn.templateId = reader.D();
			spawn.objectId = reader.D();
			spawn.x = reader.F();
			spawn.y = reader.F();
			spawn.z = reader.F();
			spawn.targetX = reader.F();
			spawn.targetY = reader.F();
			spawn.targetZ = reader.F();
			spawn.heading = reader.C();
			spawn.masterObjectId = reader.D();
			spawn.decoration = readAppearance(reader);
			pet.spawn = std::move(spawn);
			break;
		}
		case PET_ACTION_DISMISS: // :188-191
			pet.petObjectId = reader.D();
			pet.animationId = reader.C();
			break;
		case PET_ACTION_FOOD: // :192-237
			reader.expectH(1, "SM_PET FOOD writeH(1)");
			reader.expectC(1, "SM_PET FOOD writeC(1)");
			pet.subType = reader.C();
			pet.value1 = reader.D(); // getFeedProgress().getDataForPacket() in every arm
			switch (pet.subType) {
				case 1: // eat
				case 8: // is full (8: the refeed delay instead of the 0)
					for (int i = 0; i < 3; i++)
						pet.values.push_back(reader.D());
					break;
				case 2: // eating successful
					for (int i = 0; i < 3; i++)
						pet.values.push_back(reader.D());
					reader.expectC(0, "SM_PET FOOD 2 writeC(0)");
					break;
				case 3:
				case 4:
				case 5:
					pet.values.push_back(reader.D()); // the refeed delay in seconds
					break;
				case 6: // give item
					pet.values.push_back(reader.D());
					pet.values.push_back(reader.D());
					reader.expectC(0, "SM_PET FOOD 6 writeC(0)");
					break;
				case 7: // present notification
					for (int i = 0; i < 3; i++)
						pet.values.push_back(reader.D());
					break;
				default:
					break; // the switch writes nothing else
			}
			break;
		case PET_ACTION_RENAME: // :238-241
			pet.petObjectId = reader.D();
			pet.newName = reader.S();
			break;
		case PET_ACTION_MOOD: // :242-276: an unknown sub type writes nothing, not even the sub type
			if (reader.remaining() == 0)
				break;
			pet.subType = reader.C();
			switch (pet.subType) {
				case 0: // check pet status
				case 3: // give gift
					pet.value1 = reader.D();
					break;
				case 2: // emotion sent: 0, the mood points, the emotion
					reader.expectD(0, "SM_PET MOOD 2 writeD(0)");
					pet.value1 = reader.D();
					pet.values.push_back(reader.D());
					break;
				case 4: // periodic update: the points, the mood and gift remaining times
					pet.value1 = reader.D();
					pet.values.push_back(reader.D());
					pet.values.push_back(reader.D());
					break;
				default:
					reader.fail("SM_PET MOOD writes no sub type " + std::to_string(pet.subType));
			}
			break;
		case PET_ACTION_SPECIAL_FUNCTION: // :277-311
			pet.subType = reader.C();
			if (pet.subType == 2) {
				const uint8_t dopeAction = reader.C();
				pet.values.push_back(dopeAction);
				const int ints = dopeAction == 0 || dopeAction == 2 ? 2 : dopeAction == 1 || dopeAction == 3 ? 1 : 0;
				for (int i = 0; i < ints; i++)
					pet.values.push_back(reader.D());
			} else if (pet.subType == 3) {
				const uint8_t first = reader.C();
				pet.values.push_back(first);
				if (first == 0)
					pet.values.push_back(reader.C()); // the activation: writeC(0), writeC(isActing)
				else
					pet.values.push_back(reader.D()); // a looted npc: writeC(1 or 2), writeD(npcObjId)
			} else if (pet.subType == 4) {
				reader.expectC(0, "SM_PET SPECIAL_FUNCTION 4 writeC(0)");
				pet.values.push_back(reader.C());
			}
			break;
		default:
			break; // TALK_WITH_MERCHANT, TALK_WITH_MINDER, H_ADOPT, H_ABANDON: the action alone
	}
	reader.expectFullyConsumed();
	return pet;
}

KiskUpdate decodeKiskUpdate(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_KISK_UPDATE");
	KiskUpdate update;
	update.kiskObjectId = reader.D();
	update.creatorId = reader.D();
	update.useMask = reader.D();
	update.currentMembers = reader.D();
	update.maxMembers = reader.D();
	update.remainingResurrects = reader.D();
	update.maxResurrects = reader.D();
	update.remainingLifetimeSeconds = reader.D();
	reader.expectFullyConsumed();
	return update;
}

} // namespace aion::gameserver::scenario::decoders
