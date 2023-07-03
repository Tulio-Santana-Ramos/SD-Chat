#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <bits/stdc++.h>

#include "config.h"
#include "TFTP.h"

#define LIMITE_MENSAGEM 4097    // Limite do tamanho de mensagem
#define MAX_CLIENTS 100         // Número máximo de conexões suportadas

using namespace std;

struct Channel;

// Struct representando o Cliente da aplicação
struct Client{
    struct sockaddr_in address;
    int fd_cliente;
    char nickname[50];
    bool adm;
    bool muted;
    bool shutdown;
    Channel* canal_atual;
};

// Struct representando os Canais da aplicação
struct Channel{
    string name;
    int fd_admin;
    vector <Client *> usuarios;
};

typedef struct {
    Client * cliente;
    TFTP_Header* file_header;
    std::map<int,bool> file_chunks;
}file_transfer_args;

// Vetor de clientes associados ao servidor
vector <Client *> conectados;

// Vetor de clientes associados ao servidor tftp
vector <Client *> conectados_tftp;

// Vetor de canais associados ao servidor
vector <Channel *> canais;

// Mutex para sincronização de operações de clientes
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

// Mutex para sincronização de operações de clientes tftp
pthread_mutex_t clients_tftp_mutex = PTHREAD_MUTEX_INITIALIZER;

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

void server_tftp_client_remove(int fd){
    pthread_mutex_lock(&clients_tftp_mutex);

    for(uint i = 0; i < conectados_tftp.size(); i++){
        if(conectados_tftp[i]->fd_cliente == fd){
            conectados_tftp.erase(conectados_tftp.begin() + i);
            break;
        }
    }
    pthread_mutex_unlock(&clients_tftp_mutex);
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
    cliente->canal_atual = NULL;

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
                cliente->canal_atual = canal;
            }
        }

        // Caso de inserção em novo canal
        if (cliente->canal_atual == NULL) {
            cliente->canal_atual = new Channel;
            cliente->canal_atual->fd_admin = cliente->fd_cliente;
            cliente->canal_atual->usuarios.push_back(cliente);
            cliente->canal_atual->name = nome_canal;
            cliente->adm = true;
            cliente->muted = false;
            canais.push_back(cliente->canal_atual);
        }
        // Envio de mensagem ao log do servidor e broadcast para possíveis membros do canal
        sprintf(mensagem, "%s entrou", cliente->nickname);
        cout << mensagem << " em " << cliente->canal_atual->name << endl;
        send_message(mensagem, cliente->fd_cliente, cliente->canal_atual);
    }

    // Limpeza do buffer
    bzero(mensagem, LIMITE_MENSAGEM);

    // Loop principal
    while(!leave_flag){
        // Possível recepção de mensagem ou comando
        int receive = recv(cliente->fd_cliente, mensagem, LIMITE_MENSAGEM, 0);
        cout << "MENSAGEM=" << mensagem << "\n";

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
                    int user_fd = get_user_fd_by_nickname(cliente->canal_atual, parametro);
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
                    int user_fd = get_user_fd_by_nickname(cliente->canal_atual, parametro);
                    if (user_fd == -1) {
                        // Mensagem para log do servidor
                        cout << "Esse usuario nao se encontra no canal!\n";
                    } else {
                        for (auto user : cliente->canal_atual->usuarios) {
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
                    int user_fd = get_user_fd_by_nickname(cliente->canal_atual, parametro);
                    if(user_fd == -1){
                        cout << "Esse usuario nao se encontra no canal!\n";
                    }else{
                        mute_user(cliente->canal_atual, user_fd);
                    }
                }
            }
            // Comando unmute
            else if(aux.find("/unmute") != string::npos){
                if(cliente->adm){
                    int user_fd = get_user_fd_by_nickname(cliente->canal_atual, parametro);
                    if(user_fd == -1){
                        cout << "Esse usuario nao se encontra no canal!\n";
                    }else{
                        unmute_user(cliente->canal_atual, user_fd);
                    }
                }
            }
            else if (aux.find("/file") != string::npos) {
                std::cout << "/file command\n";
            }
            // Caso de envio de mensagens normais
            else if (strlen(mensagem) > 0){
                char buffer[LIMITE_MENSAGEM]; 
                strcpy(buffer, cliente->nickname);
                strcat(buffer, ": ");
                strcat(buffer, mensagem);
                // Verificação do status mute do usuário
                if(!cliente->muted){
                    send_message(buffer, cliente->fd_cliente, cliente->canal_atual);
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
            send_message(mensagem, cliente->fd_cliente, cliente->canal_atual);
            leave_flag = 1;
            user_remove(cliente->canal_atual, cliente->fd_cliente);
            if (cliente->canal_atual->usuarios.size() == 0) {
                channel_remove(cliente->canal_atual->name);
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
            user_remove(cliente->canal_atual, removed_fd);
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

bool check_file_chunks(std::map<int,bool> file_chunks)
{
    for(auto const &c : file_chunks)
    {
        if(!c.second) return false;
    }
    return true;
}

void file_chunks_status(std::map<int,bool> file_chunks)
{
    for(auto const &c : file_chunks)
    {
        std::cout << c.first << ':' << c.second << '\n';
    }
    return;
}

int get_next_chunk(int position, std::map<int,bool> file_chunks)
{
    for (size_t i = position; i < file_chunks.size(); i++)
    {
        if(!file_chunks[i]) return i;
    }
    
    return -1;
}

void* tftp_read_send(void* arg)
{
    file_transfer_args* args = (file_transfer_args*)arg;
    TFTP_Header *header = args->file_header;
    Client* cliente = args->cliente;
    
    unsigned char mensagem_cliente[LIMITE_MENSAGEM];
    unsigned short block;

    size_t restantes = args->file_chunks.size();
    int numero_maximo_envios = 10;
    int contador_de_envios = 0;
    std::fstream file("server-"+header->file, fstream::in);
    while (!check_file_chunks(args->file_chunks))
    {
        size_t contador = 0;
        sleep(1);
        for (size_t i = 0; i < args->file_chunks.size(); i++)
        {
            if(!args->file_chunks[i])
            {
                contador++;
                create_data_block(i+1,header->file_size,file, mensagem_cliente);
                if(check_op(mensagem_cliente) != TFTP_OPCODE_DATA)
                {
                    std::cout << "falha ao criar bloco de dados:\n";
                    std::cout << check_op(mensagem_cliente) << '\n';
                }

                size_t bytes = send(cliente->fd_cliente, mensagem_cliente, msg_len(mensagem_cliente) +1 , MSG_NOSIGNAL);
                if (bytes == -1)
                    std::cout << "falha ao enviar o chunk " << i <<'\n';
            }
        }
        if(contador_de_envios == numero_maximo_envios) break;
        if(contador == restantes) 
        {
            // std::cout << "Restam " << contador << " blocos para serem enviados\n";
            contador_de_envios++;
            // std::cout << "Tentativas restantes: " << (numero_maximo_envios - contador_de_envios) << "\n";
        }
        else 
        {
            restantes = contador;
            contador_de_envios = 0;   
        }
    }
    return NULL;
}

void* tftp_read_recv(void* arg)
{
    file_transfer_args* args = (file_transfer_args*)arg;
    TFTP_Header *header = args->file_header;
    Client* cliente = args->cliente;

    unsigned char mensagem_servidor[LIMITE_MENSAGEM];
    unsigned short block = 0;
    while (!check_file_chunks(args->file_chunks))
    {
        // Tratamento do buffer
        memset(mensagem_servidor, 0, sizeof(mensagem_servidor));
        block = 0;

        int recv_response = recv(
            cliente->fd_cliente,
            mensagem_servidor,
            sizeof(mensagem_servidor),
            MSG_NOSIGNAL
        );
        if(ack_sucess(mensagem_servidor,&block) && block > 0)
        {
            args->file_chunks[block-1] = true;
        }
    }
    return NULL;
}


bool tftp_read_request(TFTP_Header *header, Client* cliente)
{
    std::cout << "tftp_read_request\n";
    std::map<int,bool> file_chunks;
    pthread_t send_file_thread;
    pthread_t recv_response_thread;
    long long int file_size;
    size_t chunks;
    
    try
    {
        file_size = filesize("server-"+header->file);
        file_chunks = create_file_map(file_size);
        chunks = file_chunks.size();
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return false;
    }

    unsigned char header_string[LIMITE_MENSAGEM];
    create_header(2,header->file,file_size,header_string);
    if (!(send(cliente->fd_cliente, header_string, msg_len(header_string), MSG_NOSIGNAL) == -1))
    {
        unsigned char mensagem_servidor[LIMITE_MENSAGEM];
        
        int recv_response = recv(
            cliente->fd_cliente,
            mensagem_servidor,
            sizeof(mensagem_servidor),
            MSG_NOSIGNAL
        );

        unsigned short block = 1;
        if(ack_sucess(mensagem_servidor, &block) && block == 0)
        {                 
            decode_header(header_string,header);
        }
    }

    file_transfer_args args;
    args.cliente = cliente;
    args.file_header = header;
    args.file_chunks = file_chunks;

    sleep(1);
    pthread_create(&send_file_thread, NULL, &tftp_read_send, (void*)&args);
    pthread_create(&recv_response_thread, NULL, &tftp_read_recv, (void*)&args);
    pthread_join(send_file_thread,NULL);
    pthread_join(recv_response_thread,NULL);
    std::cout << "read complete\n";
    return true;
}

void tftp_write_request(TFTP_Header *header, Client* cliente)
{
    std::cout << "tftp_write_request\n";
    std::map<int,bool> file_chunks;
    file_chunks = create_file_map(header->file_size);
    unsigned char* buffer = (unsigned char*)malloc(header->file_size);    
        
    unsigned char mensagem_cliente[LIMITE_MENSAGEM];
    unsigned char mensagem_servidor[LIMITE_MENSAGEM];
    unsigned short block = -1;
    while (!check_file_chunks(file_chunks))
    {
        // Tratamento do buffer
        memset(mensagem_cliente, 0, sizeof(mensagem_cliente));
        memset(mensagem_servidor, 0, sizeof(mensagem_servidor));

        // recv data
        block = 0;
        int recv_response = recv(
            cliente->fd_cliente,
            mensagem_cliente,
            sizeof(mensagem_cliente),
            MSG_NOSIGNAL
        );

        // check chunk
        if(check_op(mensagem_cliente) == TFTP_OPCODE_ERR)
            std::cout << "Err\n";
        else
        {
            TFTP_Data data;
            bool success = get_data(mensagem_cliente, &data);            
            // write file
            success = write_chunk(buffer,data,*header);
            // mark true
            file_chunks[data.block-1] = true;
            create_ack_string(data.block,mensagem_servidor);
            // ack
            if(write(cliente->fd_cliente, mensagem_servidor, msg_len(mensagem_servidor)) < 0){
                perror("Erro no write\n");
                break;
            }               
        }
    // check chunks 
    }

    // write buffer;
    std::fstream file("server-"+header->file, fstream::out);
    for (size_t i = 0; i < header->file_size; i++)
    {
        file << buffer[i];
    }
    file.close();
    free(buffer);
    std::cout << "write complete\n";
    return;
}

// Lógica da thread principal de comunicação Servidor e Cliente
void *handle_client_tftp(void *arg){
    unsigned char mensagem[LIMITE_MENSAGEM];
    int leave_flag = 0;

    // Inicialização de possíveis novos clientes e canais
    Client *cliente = (Client *)arg;
    Channel *canal_atual = NULL;

    // Loop principal
    while(!leave_flag){
        // Possível recepção de mensagem ou comando
        int receive = recv(cliente->fd_cliente, mensagem, LIMITE_MENSAGEM, 0);

        if(receive > 0){
            unsigned char response[7];
            memset(response,0,7);
            create_ack_string(0, response);
            cout << "MENSAGEM_TFTP=";
            print_header(mensagem);
            TFTP_Header header;
            if(check_op(mensagem) == TFTP_OPCODE_RRQ && decode_header(mensagem,&header))
            {
                if(write(cliente->fd_cliente, response, msg_len(response)) < 0){
                    perror("Erro no write\n");
                    break;
                }               
                tftp_read_request(&header,cliente);
                leave_flag = 1;
            }
            if(check_op(mensagem) == TFTP_OPCODE_WRQ && decode_header(mensagem,&header))
            {
                if(write(cliente->fd_cliente, response, msg_len(response)) < 0){
                    perror("Erro no write\n");
                    break;
                }               
                tftp_write_request(&header,cliente);
                leave_flag = 1;
            }
            // Obter mensagem, converter e splitar para casos de possíveis parâmetros
            string aux = convert_char_to_string((char*)mensagem);
        }
        // bzero(mensagem, LIMITE_MENSAGEM);
            string parametro = split((char*)mensagem, ' ');
    }
    server_tftp_client_remove(cliente->fd_cliente);
    // Finalização da thread
    pthread_detach(pthread_self());

    return NULL;
}

void *tftp_server(void *arg)
{
    int fd_servidor;
    int option = 1;
    sockaddr_in endereco_servidor, endereco_cliente;
    pthread_t tid;

    // Criação do socket do servidor:
    fd_servidor = socket(AF_INET, SOCK_STREAM, 0);

    // Verificação se o socket foi devidamente criado:
    if (fd_servidor == -1) {
        cerr << "Erro ao criar o socket tftp!\n";
        exit(-1);
    }
    cout << "Socket tftp criado com sucesso!\n";

    // Configuração do endereço do servidor:
    endereco_servidor.sin_family = AF_INET;
    endereco_servidor.sin_port = htons(config::TFTP_SERVER_PORT);
    endereco_servidor.sin_addr.s_addr = inet_addr(config::TFTP_SERVER_IP);

    if(setsockopt(fd_servidor, SOL_SOCKET,(SO_REUSEPORT | SO_REUSEADDR),(char*)&option,sizeof(option)) < 0){
		perror("ERROR: setsockopt failed\n");
        return NULL; 
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
        if(int(conectados_tftp.size()) == MAX_CLIENTS){
            printf("Max clients reached.\n");
            close(connfd);
            continue;
        }

        // Criação de usuário
        Client *cliente = (Client *)malloc(sizeof(Client));
        cliente->address = endereco_cliente;
        cliente->fd_cliente = connfd;

        // Inserção de cliente em lista de conexões    
        pthread_mutex_lock(&clients_tftp_mutex);
        conectados_tftp.push_back(cliente);
        pthread_mutex_unlock(&clients_tftp_mutex);
        // Criação da thread
        pthread_create(&tid, NULL, &handle_client_tftp, (void *)cliente);
        sleep(1);
    }

    return NULL;
}

int main(void) {
    int fd_servidor;
    int option = 1;
    sockaddr_in endereco_servidor, endereco_cliente;
    pthread_t tid;
    pthread_t tftp_thread;

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

    // criação do servidor tftp
    pthread_create(&tftp_thread, NULL, &tftp_server,NULL);

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