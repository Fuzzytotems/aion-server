#pragma once

#include "aion/gameserver/model/stats/calc/fwd.h"

namespace aion::gameserver::model::stats::calc {

/**
 * Marker interface of everything that adds stat functions to a creature (effects, items, titles, buffs).
 * <p>
 * C++: an interface held by `Ref<StatOwner>` (fieldmap: StatFunctionProxy.owner, CreatureGameStats function owners), so it declares the
 * reference count operations (docs/design/hub-headers.md §9.2). The first implementor of each hierarchy with a runtime base forwards them to
 * RefCounted/OwnedPart; static data implementors (TitleTemplate, ItemSetTemplate) are immortal and implement them as no-ops. Written with the
 * S0b stats group because Effect (a hub) implements it.
 * <p>
 * Static data owners: the stat APIs take non-const `Ptr<StatOwner>`/`StatOwner&` and store `Ref<StatOwner>` (fieldmap), while templates are
 * `const X*`. Callers passing a template (TitleService, ItemSetService) use `const_cast<TitleTemplate*>(tpl)`: the interface has no mutating
 * member, the no-op retain/release never touch the template, and owners are only compared by identity, so the cast is safe (S0b decision; a
 * `Ref<const StatOwner>` layout would force const_casts in every Effect/Item body instead).
 *
 * @author ATracer
 */
class StatOwner {
public:
	/** C++ only: Ref<StatOwner> retains the implementing object (hub-headers.md §9.2). */
	virtual void retain() const noexcept = 0;
	virtual void release() const noexcept = 0;

	virtual ~StatOwner() = default;

protected:
	StatOwner() = default;
	StatOwner(const StatOwner&) = default;
	StatOwner& operator=(const StatOwner&) = default;
};

} // namespace aion::gameserver::model::stats::calc
