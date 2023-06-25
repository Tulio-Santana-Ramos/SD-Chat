#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <bits/stdc++.h>

#define LIMITE_MENSAGEM 4097    // Limite do tamanho de mensagem
#define MAX_CLIENTS 100         // Número máximo de conexões suportadas

using namespace std;

// Struct representando o Cliente da aplicação
struct Client{
    struct sockaddr_in address;
    int fd_cliente;
    char nickname[50];
    bool adm;
    bool muted;
    bool shutdown;
};

// Struct representando os Canais da aplicação
struct Channel{
    string name;
    int fd_admin;
    vector <Client *> usuarios;
};

// Vetor de clientes associados ao servidor
vector <Client *> conectados;

// Vetor de canais associados ao servidor
vector <Channel *> canais;

// Mutex para sincronização de operações de clientes
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

// Mutex para sincronização de operações de canais
pthread_mutex_t channels_mutex = PTHREAD_MUTEX_INITIALIZER;

// Função auxiliar para divisão de strings
string split(char str[], char delim){
    int i = 0, start = 0, end = int(strlen(str)) - 1;

    while(i++ < int(strlen(str))){
        if(str[i] == delim)
            start = i + 1;
    }
    string new_str = "";
    new_str.append(str, start, end - start + 1);
    return new_str;
}

// Função auxiliar para converter char* para string
string convert_char_to_string(char str[]){
    string new_string(str);
    return new_string;
}

// Função auxiliar para converter string para char*
char *convert_string_to_char(string str) {
    char *new_string = (char *) calloc(str.length() + 1, 1);
    strcpy(new_string, str.c_str());
    return new_string;
}

// Função auxiliar para remover clientes conectados ao servidor
void server_client_remove(int fd){
    pthread_mutex_lock(&clients_mutex);

    for(uint i = 0; i < conectados.size(); i++){
        if(conectados[i]->fd_cliente == fd){
            conectados.erase(conectados.begin() + i);
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

// Função auxiliar para remoção de canais
void channel_remove(string channel_name) {
    pthread_mutex_lock(&channels_mutex);

    for(uint i = 0; i < canais.size(); i++){
        if(canais[i]->name == channel_name){
            canais.erase(canais.begin() + i);
            break;
        }
    }
    pthread_mutex_unlock(&channels_mutex);
}

// Função auxiliar para remoção de clientes de canais
void user_remove(Channel *canal, int fd_cliente) {
    pthread_mutex_lock(&channels_mutex);

    for(uint i = 0; i < canal->usuarios.size(); i++){
        if(canal->usuarios[i]->fd_cliente == fd_cliente){
            canal->usuarios.erase(canal->usuarios.begin() + i);
            break;
        }
    }
    pthread_mutex_unlock(&channels_mutex);
}

// Função auxiliar para obter fd de usuário através de seu nickname
int get_user_fd_by_nickname(Channel *canal, string nickname) {
    for (auto user : canal->usuarios) {
        if (user->nickname == nickname)
            return user->fd_cliente;
    }
    return -1;
}

// Função auxiliar para mutar usuário a partir de seu fd
void mute_user(Channel *canal, int fd){
    for(auto user : canal->usuarios){
        if(user->fd_cliente == fd)
            user->muted = true;
    }
}

// Função auxiliar para desmutar usuário a partir de seu fd
void unmute_user(Channel *canal, int fd){
    for(auto user : canal->usuarios){
        if(user->fd_cliente == fd)
            user->muted = false;
    }
}

// Função auxiliar para broadcast em determinado canal
void send_message(char mensagem[], int fd, Channel* canal){
    pthread_mutex_lock(&clients_mutex);

    strcat(mensagem, "\n");

    for(auto cliente : canal->usuarios){
        if(cliente->fd_cliente != fd){
            if(write(cliente->fd_cliente, mensagem, strlen(mensagem)) < 0){
                perror("Erro no write\n");
                break;
            }
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

// Lógica da thread principal de comunicação Servidor e Cliente
void *handle_client(void *arg){
    char mensagem[LIMITE_MENSAGEM];
    char name[50];
    char channel[50];
    int leave_flag = 0;

    // Inicialização de possíveis novos clientes e canais
    Client *cliente = (Client *)arg;
    Channel *canal_atual = NULL;

    // Recepção do nickname inicial do usuário
    if(recv(cliente->fd_cliente, name, 50, 0) <= 0 || strlen(name) < 2 || strlen(name) >= 50 - 1){
        printf("Não inseriu nome\n");
        leave_flag = 1;
    } else {
        stpcpy(cliente->nickname, name);
    }
    bzero(mensagem, LIMITE_MENSAGEM);

    // Recepção do canal em que o usuário irá se conectar
    if(recv(cliente->fd_cliente, channel, 50, 0) <= 0 || strlen(channel) < 2 || strlen(channel) >= 50 - 1){
        printf("Não inseriu canal\n");
        leave_flag = 1;
    } else{
        string nome_canal = split(channel, ' ');

        // Caso de inserção em canal existente
        for (auto canal : canais) {
            if (canal->name == nome_canal) {
                canal->usuarios.push_back(cliente);
                canal_atual = canal;
            }
        }

        // Caso de inserção em novo canal
        if (canal_atual == NULL) {
            canal_atual = new Channel;
            canal_atual->fd_admin = cliente->fd_cliente;
            canal_atual->usuarios.push_back(cliente);
            canal_atual->name = nome_canal;
            cliente->adm = true;
            cliente->muted = false;
            canais.push_back(canal_atual);
        }
        // Envio de mensagem ao log do servidor e broadcast para possíveis membros do canal
        sprintf(mensagem, "%s entrou", cliente->nickname);
        cout << mensagem << " em " << canal_atual->name << endl;
        send_message(mensagem, cliente->fd_cliente, canal_atual);
    }

    // Limpeza do buffer
    bzero(mensagem, LIMITE_MENSAGEM);

    // Loop principal
    while(!leave_flag){
        // Possível recepção de mensagem ou comando
        int receive = recv(cliente->fd_cliente, mensagem, LIMITE_MENSAGEM, 0);

        // Variável auxiliar para casos de remoção de usuário (comando kick)
        int removed_fd = -1;
        if(receive > 0){
            // Obter mensagem, converter e splitar para casos de possíveis parâmetros
            string aux = convert_char_to_string(mensagem);
            string parametro = split(mensagem, ' ');

            // Comando ping
            if (strcmp(mensagem, "/ping") == 0) {
                strcpy(mensagem, "Pong!");
                write(cliente->fd_cliente, mensagem, strlen(mensagem));
            }
            // Comando de alteração de nickname
            else if (aux.find("/nickname") != string::npos) {
                strcpy(cliente->nickname, convert_string_to_char(parametro));
            }
            // Comando de kick
            else if (aux.find("/kick") != string::npos) {
                if (cliente->adm) {
                    int user_fd = get_user_fd_by_nickname(canal_atual, parametro);
                    if (user_fd == -1) {
                        // Mensagem para log do servidor
                        cout << "Esse usuario nao se encontra no canal!\n";
                    } else {
                        removed_fd = user_fd;
                    }
                }
            }
            // Comando whois
            else if(aux.find("/whois") != string::npos){
                if (cliente->adm) {
                    int user_fd = get_user_fd_by_nickname(canal_atual, parametro);
                    if (user_fd == -1) {
                        // Mensagem para log do servidor
                        cout << "Esse usuario nao se encontra no canal!\n";
                    } else {
                        for (auto user : canal_atual->usuarios) {
                            if (user->nickname == parametro)
                                strcpy(mensagem, inet_ntoa(user->address.sin_addr));
                        }
                        // Envio somente para administrador do canal
                        write(cliente->fd_cliente, mensagem, strlen(mensagem));
                    }
                }
            }
            // Comando mute
            else if(aux.find("/mute") != string::npos){
                if(cliente->adm){
                    int user_fd = get_user_fd_by_nickname(canal_atual, parametro);
                    if(user_fd == -1){
                        cout << "Esse usuario nao se encontra no canal!\n";
                    }else{
                        mute_user(canal_atual, user_fd);
                    }
                }
            }
            // Comando unmute
            else if(aux.find("/unmute") != string::npos){
                if(cliente->adm){
                    int user_fd = get_user_fd_by_nickname(canal_atual, parametro);
                    if(user_fd == -1){
                        cout << "Esse usuario nao se encontra no canal!\n";
                    }else{
                        unmute_user(canal_atual, user_fd);
                    }
                }
            }
            // Caso de envio de mensagens normais
            else if (strlen(mensagem) > 0){
                char buffer[LIMITE_MENSAGEM]; 
                strcpy(buffer, cliente->nickname);
                strcat(buffer, ": ");
                strcat(buffer, mensagem);
                // Verificação do status mute do usuário
                if(!cliente->muted){
                    send_message(buffer, cliente->fd_cliente, canal_atual);
                    cout << buffer << "\n";
                }else{
                    // Caso mutado o usuário receberá mensagem exclusiva para alerta-lo
                    strcpy(mensagem, "Você não pode enviar mensagens no canal pois o adm te mutou\n");
                    write(cliente->fd_cliente, mensagem, strlen(mensagem));
                }
            }
        }
        // Comando quit
        else if(receive == 0 || strcmp(mensagem, "/quit") == 0){
            sprintf(mensagem, "%s saiu do canal", cliente->nickname);
            cout << mensagem << endl;
            send_message(mensagem, cliente->fd_cliente, canal_atual);
            leave_flag = 1;
            user_remove(canal_atual, cliente->fd_cliente);
            if (canal_atual->usuarios.size() == 0) {
                channel_remove(canal_atual->name);
            }
        }
        // Casos de erro
        else{
            cout << "Erro -1\n" << endl;
            leave_flag = 1;
        }
        bzero(mensagem, LIMITE_MENSAGEM);

        // Caso necessária remoção de algum usuário
        if (removed_fd != -1) {
            user_remove(canal_atual, removed_fd);
            server_client_remove(removed_fd);
            strcpy(mensagem, "O adm te removeu deste canal :(\n");
            // Mensagem exclusiva alertando a remoção forçada do canal
            write(removed_fd, mensagem, strlen(mensagem));
        }
    }
    server_client_remove(cliente->fd_cliente);
    // Finalização da thread
    pthread_detach(pthread_self());

    return NULL;
}

int main(void) {

    int fd_servidor;
    int option = 1;
    sockaddr_in endereco_servidor, endereco_cliente;
    pthread_t tid;

    // Criação do socket do servidor:
    fd_servidor = socket(AF_INET, SOCK_STREAM, 0);

    // Verificação se o socket foi devidamente criado:
    if (fd_servidor == -1) {
        cerr << "Erro ao criar o socket!\n";
        exit(-1);
    }
    cout << "Socket criado com sucesso!\n";

    // Configuração do endereço do servidor:
    endereco_servidor.sin_family = AF_INET;
    endereco_servidor.sin_port = htons(2000);
    endereco_servidor.sin_addr.s_addr = inet_addr("127.0.0.1");

    // Handler 
    signal(SIGPIPE, SIG_IGN);

    if(setsockopt(fd_servidor, SOL_SOCKET,(SO_REUSEPORT | SO_REUSEADDR),(char*)&option,sizeof(option)) < 0){
		perror("ERROR: setsockopt failed\n");
        return -1;
	}

    // Atribuição do endereço ao socket:
    if (bind(fd_servidor, (sockaddr *) &endereco_servidor, sizeof(endereco_servidor)) == -1) {
        cerr << "Erro ao realizar bind do socket!\n";
        exit(-1);
    } else {
        cout << "Bind realizado com sucesso!\n";
    }

    // Marcação do socket para a escuta de um único cliente:
    if (listen(fd_servidor, MAX_CLIENTS) == -1) {
        cerr << "Erro na marcação de escuta por clientes!\n";
        exit(-1);
    } else {
        cout << "Servidor escutando por requisições!\n";
    }

    int connfd = 0;

    // Loop principal
    while(1){
        socklen_t clilen = sizeof(endereco_cliente);
        connfd = accept(fd_servidor, (struct sockaddr*)&endereco_cliente, &clilen);
    
        // Verificação do número de clientes conectados
        if(int(conectados.size()) == MAX_CLIENTS){
            printf("Max clients reached.\n");
            close(connfd);
            continue;
        }

        // Criação de usuário
        Client *cliente = (Client *)malloc(sizeof(Client));
        cliente->address = endereco_cliente;
        cliente->fd_cliente = connfd;
        // Inserção de cliente em lista de conexões
        pthread_mutex_lock(&clients_mutex);
        conectados.push_back(cliente);
        pthread_mutex_unlock(&clients_mutex);
        // Criação da thread
        pthread_create(&tid, NULL, &handle_client, (void *)cliente);
        sleep(1);
    }

    return 0;
}
