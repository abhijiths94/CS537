all: clean mapreduce

dbg:
	gcc -o mapreduce test.c mapreduce.c mapreduce.h  -g -Wall -Werror -pthread -O
mapreduce:
	gcc -o mapreduce -g -pg test.c mapreduce.c mapreduce.h -Wall -Werror -pthread -O

memchk:
	gcc -fsanitize=address -o mapreduce test.c mapreduce.c mapreduce.h -g -Wall -Werror -pthread -O



clean:
	rm -f *.o mapreduce

tc:
	gcc -o test_case wordcount.c mapreduce.c -Wall -Werror -pthread -O -lm -DNUM_MAPPERS=1 -DNUM_REDUCERS=1 -DNUM_ITERATION=1 -DFILE_OUTPUT_SUFFIX=\"wordcount111\"
