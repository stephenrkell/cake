package cake;

import java.io.InputStream;
import org.antlr.runtime.*;

public class CakeParserHelper extends org.antlr.runtime.tree.CommonTree implements Cloneable
{
	static cakeJavaParser getParser(InputStream stream) throws java.io.IOException
    {
        return new cakeJavaParser(new CommonTokenStream(new cakeJavaLexer(new ANTLRInputStream(stream))));
    }
}
