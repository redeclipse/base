#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

#include "cube.h"

int main(int argc, char **argv)
{
    // Generate key pair: genkey <seed>
    if(argc == 2)
    {
        vector<char> privkey, pubkey;
        genprivkey(argv[1], privkey, pubkey);
        printf("private key: %s\n", privkey.getbuf());
        printf("public key: %s\n", pubkey.getbuf());
        return EXIT_SUCCESS;
    }
    // Print yes/no to match pubkey with privkey: genkey <pubkey> <privkey>
    else if(argc == 3)
    {
        vector<char> pubkey;
        calcpubkey(argv[2], pubkey);
        printf("%s\n", !strcmp(argv[1], pubkey.getbuf()) ? "yes" : "no");
        return EXIT_SUCCESS;
    }
    return EXIT_FAILURE;
}

