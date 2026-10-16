#pragma once

#include <string>
#include <typeinfo>

namespace aion::gameserver::utils {

/**
 * Java: Class.getSimpleName() of a game server class, shown to users by commands (Id, Info, Kill, Delete, SpawnUpdate, PlayerInfo, stat
 * comparisons) and in AIEngine messages (handlers-and-porting-plan.md §1.10).
 * <p>
 * C++ class names equal the Java names, so the unqualified name of the C++ type is the Java simple name: the namespaces and enclosing classes are
 * removed ("aion::gameserver::model::gameobjects::Npc" -> "Npc", "Outer::Inner" -> "Inner") and so are template arguments, which Java's erased
 * generics do not show ("AITemplate<Npc>" -> "AITemplate"). MSVC's "class "/"struct " keywords are removed and GCC/Clang names are demangled
 * (commons getSimpleClassName).
 * <p>
 * Thread-safety: pure function.
 */
std::string simpleClassName(const std::type_info& type);

} // namespace aion::gameserver::utils
