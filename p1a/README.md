1. wis-grep

a. If lesser than 1 argument is passed, error is thrown and the program exits.
b. If exactly 1 argument is passed, we look in stdout for the expression passed.
c. If 2 or more arguments are passed the first argument is taken as the expression and the remaining arguments are file names to look in.
d. The files are opened one at a time , and is read line by line and checked for the expression. If it matches, the line is printed out else proceeds to next line.
e. If all lines are exhausted, the next file is opened and the procedure is repeated.
f. When all files are exhaisted, the program returns and returns 0 on successful completion.


2. wis-tar

a. If lesser than 2 arguments are passed, error is thrown and the program exits.
b. If 2 or more arguments are passed the first argument is taken as the tar file name and the remaining arguments are src file names to be tared.
c. For each file to be tared, we extract filename and filesize using stat and append into the tar file with the appropriate length as mentioned in the specification. Then the whole src file contents are copied and written to the tar file. This procedure is repeated till all files are exhausted. 
d. On completion, the tar file is closed and the program exits returning 0.


3. wis-untar
a. Takes exactly 1 argument and that is the tar file.
b. The file is opened for read and while the file is not empty it reads the first 100 characters to obtain the file name and creates a new file with that name.
c. The next 8 characters are used to determine the number of character to be read from the tar file and written into the newly created file. 
d. After writing into the file, if the tar file still has data it repeats the process again.


