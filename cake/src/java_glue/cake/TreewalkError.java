package cake;

public class TreewalkError extends Exception
{
	org.antlr.runtime.tree.Tree t;
	String msg;
	public TreewalkError(org.antlr.runtime.tree.Tree t, String msg)
	{
		this.t = t;
		this.msg = msg;
	}
	public String toString()
	{
		return getClass().getName() + 
			((t != null && t instanceof org.antlr.runtime.tree.CommonTree) 
				? " at input position " +  ((org.antlr.runtime.tree.CommonTree) t).getToken().getLine() +
					 ":" + ((org.antlr.runtime.tree.CommonTree) t).getCharPositionInLine()
				: "") 
				+ ": " + msg;
	}
}
