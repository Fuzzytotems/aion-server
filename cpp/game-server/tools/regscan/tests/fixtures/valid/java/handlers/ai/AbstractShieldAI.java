package ai;

import com.aionemu.gameserver.ai.AIName;
import com.aionemu.gameserver.model.gameobjects.Npc;

/**
 * @AIName("javadoc_is_ignored")
 */
@AIName("abstract_shield")
public abstract class AbstractShieldAI extends GeneralNpcAI {

	public AbstractShieldAI(Npc owner) {
		super(owner);
	}
}
