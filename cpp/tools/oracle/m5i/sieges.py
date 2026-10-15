"""m5i-sieges: the siege locations a server with sieges enabled holds and sends at login (m5i-plan.md 10.3 X1, X4).

Derived from the static data and the Java it names, never from the C++ port:
- SiegeLocationData.afterUnmarshal: in siege_locations.xml order, a FORTRESS is a location and also an artifact location (keyed by its id), an
  ARTIFACT and an OUTPOST are locations, the AGENT_FIGHT is the agent location; a repeated id replaces the earlier value in place
  (LinkedHashMap.put); outposts 2111 and 3111 start as ELYOS and ASMODIANS, every other location as BALAUR (SiegeLocation.java);
- vulnerable: artifacts only (ArtifactLocation.java:17-21); nextState: artifacts VULNERABLE (1), outposts and the agent 1 while not vulnerable
  (OutpostLocation.java:23-25, AgentLocation.java:15-17), fortresses the default 0 outside the preparation hours;
- canTeleport: true by default, and SiegeLocation.isCanTeleport(player) also needs the player's race to equal the location's.
"""
import xml.etree.ElementTree as ET
from pathlib import Path

RACE_IDS = {"ELYOS": 0, "ASMODIANS": 1, "BALAUR": 2}


def siege_report(data_dir: Path):
    root = ET.parse(Path(data_dir) / "siege" / "siege_locations.xml").getroot()
    locations = {}   # id -> entry, insertion ordered like Java's LinkedHashMap
    artifacts = {}
    for element in root.findall("siege_location"):
        loc_type = element.get("type")
        loc_id = int(element.get("id"))
        world = int(element.get("world"))
        entry = {"id": loc_id, "type": loc_type, "world": world, "race": "BALAUR", "vulnerable": False, "nextState": 0}
        if loc_type == "FORTRESS":
            locations[loc_id] = entry
            artifacts[loc_id] = {"id": loc_id, "world": world}
        elif loc_type == "ARTIFACT":
            entry["vulnerable"] = True
            entry["nextState"] = 1
            locations[loc_id] = entry
            artifacts[loc_id] = {"id": loc_id, "world": world}
        elif loc_type == "OUTPOST":
            if loc_id == 2111:
                entry["race"] = "ELYOS"
            elif loc_id == 3111:
                entry["race"] = "ASMODIANS"
            entry["nextState"] = 1
            locations[loc_id] = entry
        elif loc_type == "AGENT_FIGHT":
            entry["nextState"] = 1
            locations[loc_id] = entry
    for entry in locations.values():
        entry["raceId"] = RACE_IDS[entry["race"]]
    return {
        "locations": list(locations.values()),
        "artifacts": list(artifacts.values()),
        "raceIds": RACE_IDS,
    }
