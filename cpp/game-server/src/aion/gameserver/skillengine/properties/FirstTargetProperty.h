#pragma once

#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/model/fwd.h"
#include "aion/gameserver/skillengine/properties/fwd.h"

namespace aion::gameserver::skillengine::properties {

/**
 * Java com.aionemu.gameserver.skillengine.properties.FirstTargetProperty: picks the first target of a cast from the template's
 * `first_target` attribute (ME, TARGETORME, TARGET, MYPET, MYMASTER, PASSIVE, TARGET_MYPARTY_NONVISIBLE, POINT) and adds it to the effected
 * list.
 * <p>
 * C++: a static-only class (Java never instantiates it); declarations of m5b2-plan.md S-03, the bodies are the cast lane's.
 * - `set` takes `Skill&` although skeleton.py's draft says `Ptr<Skill>`: its only caller is Properties.validate(skill, ...), whose skill is the
 *   casting Skill itself (Skill.canUseSkill passes `this`), and no body of the family compares it with null.
 * - isTargetAllowed's `target` is nullable: the TARGET arm passes `skill.getFirstTarget()` unchecked for relation ALL, and
 *   TargetRelationProperty.isBuffAllowed answers false for null.
 *
 * @author ATracer
 */
class FirstTargetProperty {
public:
	static bool set(model::Skill& skill, const Properties* properties);

private:
	static bool isTargetTeamMember(model::Skill& skill, bool onlyGroup);

public:
	/** @return true = allow buff, false = deny buff */
	static bool isTargetAllowed(model::Skill& skill, runtime::Ptr<gameserver::model::gameobjects::Creature> target);
};

} // namespace aion::gameserver::skillengine::properties
