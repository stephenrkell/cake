package cake;

public class InternalError extends TreewalkError
{
	public InternalError(org.antlr.runtime.tree.Tree t, String msg)
	{
		super(t, msg);
	}
}
