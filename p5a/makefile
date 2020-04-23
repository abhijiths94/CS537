all: clean xfsck

dbg:
	gcc -o xfsck xfsck.c -Wall -Werror -g
xfsck:
	gcc -o xfsck xfsck.c -Wall -Werror

memchk:
	gcc -fsanitize=address -o mapreduce test.c mapreduce.c mapreduce.h -g -Wall -Werror -pthread -O



clean:
	rm -f *.o xfsck

tc:
	gcc -o test_case wordcount.c mapreduce.c -Wall -Werror -pthread -O -lm -DNUM_MAPPERS=1 -DNUM_REDUCERS=1 -DNUM_ITERATION=1 -DFILE_OUTPUT_SUFFIX=\"wordcount111\"
