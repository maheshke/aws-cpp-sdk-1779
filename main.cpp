#include "snapshot.h"

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <iostream>

#include <aws/core/Aws.h>

using namespace std;

struct Args
{
    Args() :
        snapshot(),
        outputFile()
    {
    }

    bool valid(void)
    {
        if (snapshot.empty() ||
            outputFile.empty())
        {
            printf("Invalid arguments.\n"
                    "\tsnapshot: %s\n"
                    "\toutputFile: %s\n",
                    snapshot.c_str(), outputFile.c_str()
                  );

            return false;
        }

        return true;
    }

    static void usage(void)
    {
        printf("Usage:\n"
                "./ebs_direct_poc --snapshot <EBS snapshot ID> --outputFile <output file path>\n");
    }

    string snapshot;
    string outputFile;
};

int main(int argc, char **argv)
{
    static struct option long_options[] =
    {
	{"snapshot",	required_argument,       0, 's'},
	{"outputFile",	required_argument,       0, 'o'},
	{"help",	no_argument,             0, 'h'},
	{0, 0, 0, 0}
    };

    Args args;

    while (true)
    {
        int option_index = 0;

        int c = getopt_long(argc, argv, "s:o:",
                long_options, &option_index);

        if (c == -1)
        {
            break;
        }

        switch (c)
        {
            case 's':
                args.snapshot.assign(optarg);
                break;
            case 'o':
                args.outputFile.assign(optarg);
                break;
            case 'h':
                Args::usage();
                return -1;
            default:
                printf("Invalid argument:%s\n", optarg);
                Args::usage();
                return -1;
        }
    }

    if (!args.valid())
    {
        Args::usage();
        return -1;
    }

    Aws::SDKOptions options;
    Aws::InitAPI(options);
    {
        Snapshot *snapshot = new Snapshot(args.snapshot, args.outputFile);

        ProcessExecutor executor = ProcessExecutor(snapshot);
        executor.run();
    }
    Aws::ShutdownAPI(options);

    return 0;
}
