#include "aion/gameserver/skillengine/model/Times.h"

#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"

namespace aion::gameserver::skillengine::model {

void Times::afterUnmarshal(xml::LoadContext& ctx, const xml::XmlParent& /*parent*/) {
	// Java: `weapon = weapon.intern();` saves memory, and throws NullPointerException for a missing weapon attribute. An absent String
	// attribute binds as "" (static-data.md §2.4; the census finds no present-empty weapon), so an empty weapon fails the load like Java.
	if (weapon.empty())
		ctx.fail("java.lang.NullPointerException: Times.weapon is null (missing weapon attribute)");
}

} // namespace aion::gameserver::skillengine::model
