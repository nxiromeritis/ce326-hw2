all:
	gcc -Wall mandelGUI.c mandelCore.c -L /usr/X11R6/lib -lX11 -o mandelGUI -pthread

clean:
	rm -f mandelGUI a.out

open:
	vim mandelCore.* mandelGUI.c Makefile compile.sh -p
