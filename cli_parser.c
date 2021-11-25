#include "cli_parser.h"
#include <getopt.h>

int powerOfTwo(int n)
{
    return n && (!(n & (n - 1)));
}

static int64_t
parse_int(const char *arg)
{
    char *end = NULL;
    int64_t result = strtoll(arg, &end, 10);
    if ((arg[0] == '\0') || (end == NULL) || (*end != '\0'))
        return -1;
    return result;
}

void zecho_usage()
{
    printf("\n\nzecho [EAL options] -- <Parameters>\n\n");
    printf(" -f --first_lcore         First lcore used for forwarding thread\n"
           " -n --core_num            Number of lcore used for forwarding\n"
      
           "Example:\n\n"
           "8-Thread to erspan traffic\n\n"
           "     zecho -a 0000:0e:00.1 -- --first_lcore 24 --core_num 8 \n\n");
}

int zecho_args_parser(int argc, char **argv, struct config *config)
{
    char *l_opt_arg;
    char *const short_options = "a:f:x:n:h";
    struct option long_options[] = {
        {"file-prefix", 1, NULL, 'x'},
        {"first_lcore", 1, NULL, 'f'},
        {"core_num", 1, NULL, 'n'},
        {0, 0, 0, 0},
    };

    int c;
    int64_t val;
    while ((c = getopt_long(argc, argv, short_options, long_options, NULL)) != -1)
    {
        switch (c)
        {
        // first lcore
        case 'f':
            val = parse_int(optarg);
            if (val <= 0 || val > MAX_SERVICE_CORE)
            {
                printf("Invalid firstcore value: %ld\n", val);
                zecho_usage();
                return -1;
            }
            config->first_lcore = val;
            break;
        // num of core
        case 'n':
            val = parse_int(optarg);
            if (val <= 0 || val > MAX_SERVICE_CORE)
            {
                printf("Invalid num of core value: %ld\n", val);
                zecho_usage();
                return -1;
            }
            config->num_service_core = val;
            break;

        case 'a': 
            break;
        case 'x':
            break;
        case 'h':
            zecho_usage();
            return -1;
        default:
            zecho_usage();
            return -1;
        }
    }
    return 0;
}
