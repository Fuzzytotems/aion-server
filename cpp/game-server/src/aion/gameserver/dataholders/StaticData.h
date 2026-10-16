#pragma once

#include "aion/gameserver/dataholders/StaticData.xml.h"

namespace aion::gameserver::dataholders {

/**
 * An instance of this class is the result of data loading.
 * <p>
 * C++: DataManager::loadStaticData moves the holders the loader bound into these fields (static-data.md §3.3). Java's beforeUnmarshal installs
 * the StaticDataListener that xml::LoadContext replaces; the after-unmarshal task list and the XSD validation task do not exist
 * (LoadContext::runAfterUnmarshalTask runs inline, no XSD validation at run time). afterUnmarshal logs the 90 "Loaded N ..." lines through
 * logCounts, which DataManager calls where JAXB would call the hook.
 *
 * @author Luno, orz, Wakizashi
 */
class StaticData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/StaticData.xml.inc"
public:
	/** C++ only: the body of afterUnmarshal (Java StaticData.java:315-404), logged by com.aionemu.gameserver.dataholders.StaticData */
	void logCounts() const;
};

} // namespace aion::gameserver::dataholders
