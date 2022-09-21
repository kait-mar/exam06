#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>


typedef struct s_client {
    int fd;
    int id;
    struct s_client *next;
}   t_client;

/***************************/

uint16_t PORT;
fd_set  master_set, read_set, write_set;
int fd_socket;
t_client    *clients = NULL;
char    string[100000];
char message[50];




void    error(char *s) {
    write(2, s, strlen(s));
    close(fd_socket);
    exit(1);
}

void    send_to_all(char *str, int sender) {
    t_client    *temp = clients;

    while (temp) {
        if (sender != temp->fd && FD_ISSET(temp->fd, &write_set)) {
            if (send(temp->fd, str, strlen(str), 0) < 0)
                error("fatal error\n");
            temp = temp->next;
        }
    }
}

void    spread_msg(int fd) {
    char tmp[100000];
    char    msg[100000];

    bzero(tmp, sizeof(tmp));
    bzero(msg, sizeof(msg));
    int i = 0, j = 0;
    while (string[i]) {
        if (string[i] == '\n') {
            sprintf(msg, "client %d: %s", get_id(fd), tmp);
            send_to_all(msg, fd);
            bzero(tmp, sizeof(tmp));
            bzero(msg, sizeof(msg));
            j = 0;
        }
        tmp[j++] = string[i++];
    }
    bzero(&string, strlen(string));
}

int     max_fd() {
    t_client            *temp = clients;
    int                 max = 0;

    while (temp) {
        if (temp->fd > max)
            max = temp->fd;
        temp  = temp->next;
    }
    return max;
}

void    join_client() {

    struct sockaddr_in  client_addr;
    int                 addr_len;
    int                 fd_client;
    t_client            *cl, *temp = clients;

    if ((fd_client = accept(fd_socket, (struct sockaddr*)&client_addr, (socklen_t*)&addr_len)) < 0)
        error("Fatal error\n");
    if (clients == NULL) {
        clients = malloc(sizeof(t_client));
        clients->id = 0;
        clients->fd = fd_client;
        clients->next = NULL;
        FD_SET(fd_client, &master_set);
        return ;
    }
    while (temp->next)
        temp = temp->next;
    cl = malloc(sizeof(t_client));
    cl->fd = fd_client;
    cl->id = temp->id + 1;
    cl->next = NULL;
    sprintf(message, "server: client %d just arrived\n", temp->id + 1);
    temp->next = cl;
    send_to_all(message, cl->fd);
}

int     get_id(int fd) {
    t_client    *temp = clients;

    while (temp) {
        if (temp->fd == fd)
            return temp->id;
    }
    return (-1);
}

void    delete_client(int fd) {
    t_client *temp = clients;
    t_client   *tmp;

    while (temp->next)
    {
        if (temp->next->fd == fd) {
            tmp = temp->next;
            temp->next = temp->next->next;
            FD_CLR(fd, &master_set);
            free(tmp);
        }
    }
    
}

int main(int argc, char** argv) {
    struct sockaddr_in  address;
    int     addrlen = sizeof(address);

    if (argc != 2) {
        error("Wrong number of arguments\n");
    }
    PORT = atoi(argv[1]);
    if ((fd_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        error("Fatal error\n");

    /* binding */
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(PORT);
    address.sin_addr.s_addr  =  htonl(2130706433);  //127.0.0.1
    if (bind(fd_socket, (struct sockaddr*)&address, sizeof(address)) < 0)
        error("Fatal error\n");
    // make the socket ready to listen
    if (listen(fd_socket, 10) < 0)
        error("Fatal error\n");
    //prepare the sets
    FD_ZERO(&master_set);
    FD_SET(fd_socket, &master_set);

    //make the socket accepts connections
    while (1) {
        //initialize the sets before every selection is required
        read_set = master_set;
        write_set = master_set;
        if (select(max_fd() + 1, &read_set, &write_set, NULL, NULL) < 0)
            continue;
        for (int fd = 0; fd < max_fd(); fd++) {
            if (FD_ISSET(fd, &read_set)) {
                if (fd_socket == fd) //means ready to accept a connection 
                {
                    bzero(&message, sizeof(message));
                    join_client();
                    break;
                }
                else { //means already accepted the connection and now ready to recieve message
                    int res;
                    while ((res = recv(fd, string + strlen(string), 500, 0))) //to change later
                    {
                        if (string[strlen(string) - 1 ] == '\n' && res != 500)
                            break;
                        
                    }
                    if (res <= 0) { //client left
                        bzero(&message, sizeof(message));
                        sprintf(message, "server: client %d just left\n", get_id(fd));
                        send_to_all(message, fd);
                        close(fd);
                        delete_client(fd);
                    }
                    else {
                        spread_msg(fd);
                    }
                }

            }
        }
    }

}
