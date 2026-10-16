#include "aion/gameserver/handlers/admincommands/Pet.h"

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include "aion/commons/utils/Numbers.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/PetData.h"
#include "aion/gameserver/model/templates/pet/PetFunction.h"
#include "aion/gameserver/model/templates/pet/PetTemplate.h"
#include "aion/gameserver/services/toypet/PetAdoptionService.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(Pet);

Pet::Pet()
	: AdminCommand("pet", "Adds or removes a pet.",
		  "list - Lists all available Pet IDs.\n"
		  "add <pet ID> <name> - Adds the pet with the specified ID and names it.\n"
		  "del <pet ID> - Deletes the pet with the specified ID.\n") {
}

// Java Pet.java:28-66
void Pet::execute(Player& admin, std::span<const std::string> params) {
	if (params.empty()) {
		sendInfo(admin);
		return;
	}

	const std::string& action = params[0];
	if (commons::utils::StringUtils::equalsIgnoreCase(action, "list")) {
		std::string sb = "List of pets:"; // parity= StringBuilder sb = new StringBuilder("List of pets:");
		std::vector<int32_t> ids = DataManager::PET_DATA->getPetIds(); // parity: the stream of the forEach below, sorted() by the next line
		std::sort(ids.begin(), ids.end());                              // parity: (the same statement)
		for (int32_t id : ids) { // parity= DataManager.PET_DATA.getPetIds().stream().sorted().forEach(id -> {
			const model::templates::pet::PetTemplate* template_ = DataManager::PET_DATA->getPetTemplate(id); // parity= PetTemplate template = DataManager.PET_DATA.getPetTemplate(id);
			sb += '\n'; // parity= sb.append('\n');
			sb += std::to_string(template_->getTemplateId()); // parity= sb.append(template.getTemplateId());
			sb += " - "; // parity= sb.append(" - ");
			sb += ChatUtil::color(template_->getL10n(), utils::JavaColor::WHITE); // parity= sb.append(ChatUtil.color(template.getL10n(), Color.WHITE));
			sb += "\n\tFunctions: "; // parity= sb.append("\n\tFunctions: ");
			std::vector<const model::templates::pet::PetFunction*> functions = template_->getPetFunctions(); // parity: the iterator of the for below
			for (size_t i = 0; i < functions.size(); i++) { // parity= for (Iterator<PetFunction> iter = template.getPetFunctions().iterator(); iter.hasNext(); )
				std::optional<model::templates::pet::PetFunctionType> type = functions[i]->getPetFunctionType(); // parity: iter.next() of the append below
				sb += type ? std::string(xml::enumName(*type)) : std::string("null"); // parity= sb.append(iter.next().getPetFunctionType()).append(iter.hasNext() ? ", " : "");
				sb += i + 1 < functions.size() ? ", " : ""; // parity: (the same statement)
			}
		}
		sendInfo(admin, sb); // parity= sendInfo(admin, sb.toString());
	} else {
		if (params.size() < 2) // parity: Java's ArrayIndexOutOfBoundsException of params[1], explicit
			throw runtime::ArrayIndexOutOfBoundsException("Index 1 out of bounds for length 1"); // parity: (the same; ChatCommand.run logs it)
		int32_t petId = commons::utils::parseInt(params[1]);
		if (DataManager::PET_DATA->getPetTemplate(petId) == nullptr) {
			sendInfo(admin, "Invalid pet ID.");
			return;
		}
		if (commons::utils::StringUtils::equalsIgnoreCase(action, "add")) {
			if (params.size() != 3) {
				sendInfo(admin, "You must specify a name for the pet.");
				return;
			}
			services::toypet::PetAdoptionService::addPet(admin, petId, params[2], 0, 0);
		} else if (commons::utils::StringUtils::equalsIgnoreCase(action, "del")) {
			services::toypet::PetAdoptionService::surrenderPet(admin, petId);
		}
	}
}

} // namespace aion::gameserver::handlers::admincommands
