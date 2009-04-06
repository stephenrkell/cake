package cake;

public class SemanticError extends Exception
{
	org.antlr.runtime.tree.Tree t;
	String msg;
	public SemanticError(org.antlr.runtime.tree.Tree t, String msg)
	{
		this.t = t;
		this.msg = msg;
	}
}
