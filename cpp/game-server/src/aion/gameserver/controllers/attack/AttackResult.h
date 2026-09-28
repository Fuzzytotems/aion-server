#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/skillengine/model/HitType.h"
#include "aion/gameserver/controllers/attack/fwd.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::controllers::attack {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5), written for the member-type guards waiting for it at the spine freeze.
 *
 * @author ATracer, Sippolo, kecimis
 */
class AttackResult : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	runtime::Field<float> damage{};
	const AttackStatus attackStatus;
	runtime::Field<skillengine::model::HitType> hitType{skillengine::model::HitType::EVERYHIT};
	runtime::Field<int32_t> shieldType{};
	runtime::Field<int32_t> reflectedDamage{0};
	runtime::Field<int32_t> reflectedSkillId{0};
	runtime::Field<int32_t> protectedSkillId{0};
	runtime::Field<int32_t> protectedDamage{0};
	runtime::Field<int32_t> protectorId{0};
	runtime::Field<int32_t> mpAbsorbed{0};
	runtime::Field<int32_t> mpShieldSkillId{0};
	runtime::Field<bool> launchSubEffect{true};

protected:
	AttackResult(float damage, AttackStatus attackStatus);

public:
	static runtime::Ref<AttackResult> create(float value, AttackStatus attackStatusValue);

protected:
	AttackResult(float damage, AttackStatus attackStatus, skillengine::model::HitType type);

public:
	static runtime::Ref<AttackResult> create(float value, AttackStatus attackStatusValue, skillengine::model::HitType type);

	int32_t getDamage();

	float getExactDamage() const { return this->damage.get(); }

	void setDamage(float value) { this->damage.set(value); }

	AttackStatus getAttackStatus() const { return this->attackStatus; }

	skillengine::model::HitType getHitType() const { return this->hitType.get(); }

	void setHitType(skillengine::model::HitType type) { this->hitType.set(type); }

	int32_t getShieldType() const { return this->shieldType.get(); }

	void setShieldType(int32_t shieldType);

	int32_t getReflectedDamage() const { return this->reflectedDamage.get(); }

	void setReflectedDamage(int32_t value) { this->reflectedDamage.set(value); }

	int32_t getReflectedSkillId() const { return this->reflectedSkillId.get(); }

	void setReflectedSkillId(int32_t skillId) { this->reflectedSkillId.set(skillId); }

	int32_t getProtectedSkillId() const { return this->protectedSkillId.get(); }

	void setProtectedSkillId(int32_t skillId) { this->protectedSkillId.set(skillId); }

	int32_t getProtectedDamage() const { return this->protectedDamage.get(); }

	void setProtectedDamage(int32_t value) { this->protectedDamage.set(value); }

	int32_t getProtectorId() const { return this->protectorId.get(); }

	void setProtectorId(int32_t value) { this->protectorId.set(value); }

	bool isLaunchSubEffect() const { return this->launchSubEffect.get(); }

	void setLaunchSubEffect(bool value) { this->launchSubEffect.set(value); }

	int32_t getMpAbsorbed() const { return this->mpAbsorbed.get(); }

	void setMpAbsorbed(int32_t value) { this->mpAbsorbed.set(value); }

	int32_t getMpShieldSkillId() const { return this->mpShieldSkillId.get(); }

	void setMpShieldSkillId(int32_t value) { this->mpShieldSkillId.set(value); }

protected:
	~AttackResult() override;
};

} // namespace aion::gameserver::controllers::attack
