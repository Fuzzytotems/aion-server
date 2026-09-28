package ai;

import com.aionemu.gameserver.ai.AIName;
import com.aionemu.gameserver.model.gameobjects.Npc;

/**
 * @AIName("javadoc_is_ignored")
 */
@AIName("aggressive")
public class AggressiveNpcAI extends GeneralNpcAI {

	public AggressiveNpcAI(Npc owner) {
		super(owner);
	}

	private void spawnHelpers(boolean elyos) {
		// spawn(299999)
		spawn(elyos ? 206001 : 206002, 0f, 0f, 0f);
		sp(215074, 1, 2, 3);
	}
}
