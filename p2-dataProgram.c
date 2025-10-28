#include <stdio.h>
#include <string.h>
int main_server();
int main_client();


int main(int argc, char *argv[]) {
    if (strcmp(argv[1], "server") == 0){
        return main_server();
    }else if(strcmp(argv[1], "client") == 0){
        return main_client();
    }
    return 0;
}
