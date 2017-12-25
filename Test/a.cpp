#include <stdio.h>

int foo_called(int x)
{
	printf("%d,",x);
	return x+x;
}

int foo(int k) 
{
	int __attribute__((annotate("xxx"))) a;
	
	for (int i = 0; i < k; i++)
	{
		int c = a + k;
		a += foo_called(c);
	}
	
	return 0;
}

