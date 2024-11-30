all: OS chmod

OS: main.c
	gcc -o OS main.c -lpthread
		
chmod:	
	chmod 777 OS

