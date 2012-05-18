#include <file>

main()
    {
    /* ask for a filename */
    print "Please enter a filename: "
    new filename{128}
    getstring filename

    /* try to open the file */
    new File: file = fopen(filename, io_read)
    if (!file)
        {
        printf "The file '%s' cannot be opened for reading\n", filename
        exit
        }

    /* dump the file onto the console */
    new line{200}
    while (fread(file, line))
        print line, .highlight = true

    /* done */
    fclose file
    }
