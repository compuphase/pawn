#include <file>

@start()
    {
    /* ask for a filename */
    print "Please enter a filename: "
    var filename{128}
    getstring filename

    /* try to open the file */
    var File: file = fopen(filename, io_read)
    if (!file)
        {
        printf "The file '%s' cannot be opened for reading\n", filename
        exit
        }

    /* dump the file onto the console */
    var line{200}
    while (fread(file, line))
        print line, .highlight = true

    /* done */
    fclose file
    }
