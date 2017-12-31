#include <stdio.h>


/*void foo_refer(int &x, int r)
{
	x = 10 + r;
}

int foo_called(int r)
{
        int a = 1;
	foo_refer(a, r);
	return a+1;
}*/

/*void foo_called(int* k,int y)
{
*k = y;
}*/

int foos(int k)
{
return 10+k;
}

int foo_c(int a, int r)
{
	return a+r+10;
}

int foo_called(int r)
{
        int a = 1;
	return foo_c(a,r)+1;
}

/*int sorts(int k, int& y)
{
int s[10] = {k,};
y = s[0];
return k; 
}*/

int foo(int k) 
{
	int __attribute__((annotate("xxx"))) a;
	int y = 5;
	int s = 10;
//y = 10;
//foo_called(&a,y);
//a = foos(s);
//sorts(s,a);
a = foo_called(y);
	
	//for (int i = 0; i < k; i++)
	//{
		//int c = a + k;
		//a += foo_called(c);
	//}
	
	return 0;
}

