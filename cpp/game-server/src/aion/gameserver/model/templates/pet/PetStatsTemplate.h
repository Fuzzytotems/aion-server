#pragma once

#include <string>

namespace aion::gameserver::model::templates::pet {

/**
 * Java com.aionemu.gameserver.model.templates.pet.PetStatsTemplate: a JAXB type that no unmarshal root reaches (xmlgen-report.md "Unreachable
 * JAXB-annotated types"; pet templates bind model.templates.stats.PetStatsTemplate), so nothing binds it; a value class (fieldmap K5).
 *
 * @author M@xx
 */
class PetStatsTemplate {
private:
	std::string reaction;
	float runSpeed = 0.0f;
	float walkSpeed = 0.0f;
	float height = 0.0f;
	float altitude = 0.0f;

public:
	const std::string& getReaction() const { return reaction; }

	float getRunSpeed() const { return runSpeed; }

	float getWalkSpeed() const { return walkSpeed; }

	float getHeight() const { return height; }

	float getAltitude() const { return altitude; }
};

} // namespace aion::gameserver::model::templates::pet
