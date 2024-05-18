capabilities.tab.c capabilities.tab.h:	capabilities.y
	bison -t -v -d capabilities.y

lex.yy.c: capabilities.l capabilities.tab.h
	flex capabilities.l

