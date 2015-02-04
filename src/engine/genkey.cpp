#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

#include "cube.h"

int main(int argc, char **argv)
{
    if(argc < 2) return EXIT_FAILURE;
    vector<char> privkey, pubkey;
    genprivkey(argv[1], privkey, pubkey);
    printf("private key: %s\n", privkey.getbuf());
    printf("public key: %s\n", pubkey.getbuf());
    return EXIT_SUCCESS;
}

