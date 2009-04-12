package cake;

public class SemanticError extends TreewalkError
{
	public SemanticError(org.antlr.runtime.tree.Tree t, String msg)
	{
		super(t, msg);
	}
}
