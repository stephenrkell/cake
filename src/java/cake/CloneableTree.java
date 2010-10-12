package cake;

import org.antlr.runtime.tree.*;

public class CloneableTree extends org.antlr.runtime.tree.CommonTree implements Cloneable
{
    static final long serialVersionUID = 0;
	public Object clone() throws CloneNotSupportedException { return super.clone(); }
	public CloneableTree(Tree t)
	{
		super(((CommonTree) t).getToken());
		for (int i = 0; i < t.getChildCount(); i++)
		{
			addChild(new CloneableTree(t.getChild(i)));
		}		
	}
}
