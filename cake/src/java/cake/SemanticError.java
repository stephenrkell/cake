package cake;

public class SemanticError extends TreewalkError
{
    static final long serialVersionUID = 0;
	public SemanticError(org.antlr.runtime.tree.Tree t, String msg)
	{
		super(t, msg);
	}
}
