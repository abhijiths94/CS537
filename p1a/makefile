all: clean wis-grep wis-tar wis-untar

wis-grep:
	gcc -o wis-grep wis-grep.c -Wall -Werror
wis-tar:
	gcc -o wis-tar wis-tar.c -Wall -Werror
wis-untar:
	gcc -o wis-untar wis-untar.c -Wall -Werror
clean:
	rm -f *.o wis-grep wis-tar wis-untar

