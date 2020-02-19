The flow is as follows :

- Check if running in batch mode or Interactive mode and accordingly call the shell function which handles where to read input from.

- input line

- Parse line and check if it is a multiline command or not

- If Not Multiline
    - tokenize input with delimiter as space
    - call check_cmd function to check syntax
    - if syntaxically correct, execute it

- If multiline command:
    - tokenize wrt ';' as delim
    - tokenize individual tokens from prev step with delim as '&'
    - pass this array of commands to check_cmd fn and check syntax for each.
    - if all pass syntax execute them one by one if serial in blocking mode (wait for child to finish before returning) and if parallel call execute command in non blocking mode and wait for all process after starting execution of all parallel commands.
 

