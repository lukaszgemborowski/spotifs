#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // getopt
#include "fs.h"
#include "spotify.h"
#include "context.h"
#include "logger.h"

void print_usage_and_exit(void)
{
    fprintf(stderr, "Usage is: spotifs -u username -p password /mount/point\n\n");
    exit(-1);
}

int main(int argc, char **argv)
{
    int option = 0;
    int result = EXIT_SUCCESS;
    const char* username = NULL;
    const char* password = NULL;

    while((option = getopt(argc, argv, "u:p:")) != -1)
    {
        switch(option)
        {
        case 'u':
            username = optarg;
            break;

        case 'p':
            password = optarg;
            break;

        default:
            print_usage_and_exit();
        }
    }

    if (!username || !password || argc != (1+optind))
    {
        print_usage_and_exit();
    }

    struct spotifs_context* context = get_global_context;
    memset(context, 0, sizeof(struct spotifs_context));

    // login to spotify service
    if (0 == spotify_connect(context, username, password))
    {
        logger_message(context, "Connected to spotify\n");

        // create logger (aka. log file)
        /*if (0 != logger_open(&context))
        {
            fprintf(stderr, "Can't create log file. Aborting.\n");
            exit(-2);
        }*/

        // run fuse
        char *arguments[5];
        arguments[0] = argv[0];
        arguments[1] = "-f";
        //arguments[2] = "-d";
        arguments[2] = argv[optind];
        arguments[3] = NULL;

        result = fuse_main(3, arguments, &spotifs_operations, context);

        // logout and release spotify session
        spotify_disconnect(context);
    }
    else
    {
        // login failed, fail reason is already printed in logfile
        result = -1;
    }

    logger_message(context, "Exiting..\n");
    //logger_close(context);
    return result;
}
