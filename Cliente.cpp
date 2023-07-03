#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <bits/stdc++.h>

#include "TFTP.h"
#include "config.h"

#define LIMITE_MENSAGEM 4097    // Definição do limite do tamanho da mensagem
#define LIMITE_ARQUIVO 16384    // Definição do limite do tamanho do arquivo

using namespace std;

std::map<int,bool> file_chunks;
int fd_cliente; // Número de identificação do cliente
int fd_cliente_tftp; // Número de identificação do cliente tftp
string entrada; // Entrada padrão
char nickname[50];  // Nickname do usuário
sockaddr_in endereco_servidor;
sockaddr_in endereco_servidor_tftp;
char mensagem_cliente[LIMITE_MENSAGEM];
char mensagem_servidor[LIMITE_MENSAGEM];

// std::string my_hash;

// Função auxiliar para converter char* para string
string convert_char_to_string(char str[]){
    string new_string(str);
    return new_string;
}

// Função para conexão do servidor
bool conectar_servidor(int FD,sockaddr_in endereco, const char* IP, const int PORT) {
    // Configuração da porta e IP para o mesmo endereço do servidor:
    endereco.sin_family = AF_INET;
    endereco.sin_port = htons(PORT);
    // O endereço IP abaixo pode ser setado para o local, caso rode o servidor e esta aplicação na mesma máquina
    // Assim como pode ser inserido um IP distinto, caso outra máquina na mesma rede esteja executando a aplicação de servidor
    endereco.sin_addr.s_addr = inet_addr(IP);
    // Retorno da conexão com o servidor:
    return connect(FD, (sockaddr *) &endereco, sizeof(endereco)) != -1;
}

// Função para envio de mensagem ao servidor
bool mandar_mensagem_servidor(string mensagem_total) {
    memset(mensagem_cliente, 0, LIMITE_MENSAGEM * sizeof(char));
    // Envio da mensagem em blocos de no máximo LIMITE_MENSAGEM:
    int j = 0;
    for (int i = 0; i < mensagem_total.size(); i++) {
        mensagem_cliente[j++] = mensagem_total[i];
        if (i == mensagem_total.size() - 1 || j == LIMITE_MENSAGEM - 1) {
            mensagem_cliente[j] = '\0';
            // Envia um bloco da mensagem ao servidor:
            if (send(fd_cliente, mensagem_cliente, strlen(mensagem_cliente) + 1, MSG_NOSIGNAL) == -1)
                return false;
            j = 0;
        }
    }
    // Sinaliza final do envio:
    mensagem_cliente[0] = '\0';
    if (send(fd_cliente, mensagem_cliente, 1, MSG_NOSIGNAL) == -1)
        return false;
    return true;
}

bool check_file_chunks()
{
    for(auto const &c : file_chunks)
    {
        if(!c.second) return false;
    }
    return true;
}

void file_chunks_status()
{
    for(auto const &c : file_chunks)
    {
        std::cout << c.first << ':' << c.second << '\n';
    }
    return;
}


int get_next_chunk(int position)
{
    for (size_t i = position; i < file_chunks.size(); i++)
    {
        if(!file_chunks[i]) return i;
    }
    
    return -1;
}

void* tftp_write_send(void* args)
{
    TFTP_Header *header = (TFTP_Header *) args;

    unsigned char mensagem_cliente[LIMITE_MENSAGEM];
    // char mensagem_cliente[LIMITE_MENSAGEM];
    unsigned short block;
    std::fstream file(header->file, fstream::in);
    // std::cout << file.rdbuf() << '\n';
    while (!check_file_chunks())
    {
        for (size_t i = 0; i < file_chunks.size(); i++)
        {
            if(!file_chunks[i])
            {
                create_data_block(i+1,header->file_size,file, mensagem_cliente);
                if(check_op(mensagem_cliente) != TFTP_OPCODE_DATA)
                {
                    std::cout << check_op(mensagem_cliente) << '\n';
                    break;
                }

                size_t bytes = send(fd_cliente_tftp, mensagem_cliente, msg_len(mensagem_cliente) +1 , MSG_NOSIGNAL);
                if (bytes == -1)
                    std::cout << "falha ao enviar o chunk " << i <<'\n';
            }
        }
    }
    pthread_detach(pthread_self());
    return NULL;
}

void* tftp_write_recv(void* args)
{
    unsigned char mensagem_servidor[LIMITE_MENSAGEM];
    unsigned short block = 0;
    while (!check_file_chunks())
    {
        // Tratamento do buffer
        memset(mensagem_servidor, 0, sizeof(mensagem_servidor));
        block = 0;

        int recv_response = recv(
            fd_cliente_tftp,
            mensagem_servidor,
            sizeof(mensagem_servidor),
            MSG_NOSIGNAL
        );
        
        if(ack_sucess(mensagem_servidor,&block) && block > 0)
        {
            file_chunks[block-1] = true;
        }
    }
    pthread_detach(pthread_self());
    return NULL;
}

bool read_file(TFTP_Header *header)
{
    file_chunks = create_file_map(header->file_size);
    unsigned char* buffer = (unsigned char*)malloc(header->file_size);    
    
    unsigned char mensagem_cliente[LIMITE_MENSAGEM];
    unsigned char mensagem_servidor[LIMITE_MENSAGEM];
    unsigned short block = -1;
    while (!check_file_chunks())
    {
        // Tratamento do buffer
        memset(mensagem_cliente, 0, sizeof(mensagem_cliente));
        memset(mensagem_servidor, 0, sizeof(mensagem_servidor));
    // recv data
        block = 0;
        int recv_response = recv(
            fd_cliente_tftp,
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
            size_t msg_size = write(fd_cliente_tftp, mensagem_servidor, msg_len(mensagem_servidor));
            if(msg_size < 0){
                perror("Erro no write\n");
                break;
            }               
        }
    // check chunks 
    }

    // write buffer;
    std::fstream file("client-"+string(nickname)+"-"+header->file, fstream::out);
    for (size_t i = 0; i < header->file_size; i++)
    {
        file << buffer[i];
    }
    file.close();
    free(buffer);
    return NULL;
}

bool send_file(std::string nomeArquivo)
{
    pthread_t send_file_thread;
    pthread_t recv_response_thread;
    TFTP_Header header;
    long long int file_size;
    size_t chunks;
    // std::string data;
    try
    {
        file_size = filesize(nomeArquivo);
        file_chunks = create_file_map(file_size);
        chunks = file_chunks.size();
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return false;
    }

    bool conectado = false;

    for(int i = 0; i < config::TFTP_CONNECTION_RETRY; i++)
    {
        std::cout << "Conectando ao servidor TFTP\n";
        if (conectar_servidor(fd_cliente_tftp,endereco_servidor_tftp,config::TFTP_SERVER_IP,config::TFTP_SERVER_PORT))
        {
            unsigned char header_string[LIMITE_MENSAGEM];
            create_header(2,nomeArquivo,file_size,header_string);
            if (!(send(fd_cliente_tftp, header_string, msg_len(header_string), MSG_NOSIGNAL) == -1))
            {
                unsigned char mensagem_servidor[LIMITE_MENSAGEM];
                
                int recv_response = recv(
                    fd_cliente_tftp,
                    mensagem_servidor,
                    sizeof(mensagem_servidor),
                    MSG_NOSIGNAL
                );

                unsigned short block = 1;
                if(ack_sucess(mensagem_servidor, &block) && block == 0)
                {
                    cout << "Conexão com servidor efetuada com sucesso\n";                    
                    // envio e recebimento dos arquivos
                    decode_header(header_string,&header);
                    break;
                }
                cout << "Conexão recusada\n";
                sleep(1);        
                if(i == config::TFTP_CONNECTION_RETRY)
                {        
                    cerr << "Erro ao conectar com o servidor!\n";
                    return false;
                }
            }
        }
        else
        {
            cout << "Conexão recusada\n";
            if(i == config::TFTP_CONNECTION_RETRY)
            {        
                cerr << "Erro ao conectar com o servidor!\n";
                return false;
            }
            sleep(1);
        }
    }
    pthread_create(&send_file_thread, NULL, &tftp_write_send, (void*)&header);
    pthread_create(&recv_response_thread, NULL, &tftp_write_recv, (void*)&header);
     
    return true;
}

bool get_file(std::string nomeArquivo)
{
    TFTP_Header header;
    long long int file_size = 0;
    size_t chunks;
    unsigned char mensagem_servidor[LIMITE_MENSAGEM];
    bool conectado = false;

    for(int i = 0; i < config::TFTP_CONNECTION_RETRY; i++)
    {
        std::cout << "Conectando ao servidor TFTP\n";
        if (conectar_servidor(fd_cliente_tftp,endereco_servidor_tftp,config::TFTP_SERVER_IP,config::TFTP_SERVER_PORT))
        {
            unsigned char header_string[LIMITE_MENSAGEM];
            create_header(1,nomeArquivo,file_size,header_string);
            if (!(send(fd_cliente_tftp, header_string, msg_len(header_string), MSG_NOSIGNAL) == -1))
            {
                int recv_response = recv(
                    fd_cliente_tftp,
                    mensagem_servidor,
                    sizeof(mensagem_servidor),
                    MSG_NOSIGNAL
                );

                unsigned short block = 1;
                if(ack_sucess(mensagem_servidor, &block) && block == 0)
                {
                    cout << "Conexão com servidor efetuada com sucesso\n";                    
                    decode_header(header_string,&header);
                    break;
                }
                cout << "Conexão recusada\n";
                sleep(1);        
                if(i == config::TFTP_CONNECTION_RETRY)
                {        
                    cerr << "Erro ao conectar com o servidor!\n";
                    return false;
                }
            }
        }
        else
        {
            cout << "Conexão recusada\n";
            if(i == config::TFTP_CONNECTION_RETRY)
            {        
                cerr << "Erro ao conectar com o servidor!\n";
                return false;
            }
            sleep(1);
        }
    }
    int receive = recv(fd_cliente_tftp, mensagem_servidor, LIMITE_MENSAGEM, 0);
    if(receive > 0){
        unsigned char response[7];
        memset(response,0,7);
        create_ack_string(0, response);
        TFTP_Header header;
        if(check_op(mensagem_servidor) == TFTP_OPCODE_WRQ && decode_header(mensagem_servidor,&header))
        {
            if(write(fd_cliente_tftp, response, msg_len(response)) < 0){
                perror("Erro no write\n");
            }               
            read_file(&header);
            return true;
        }
    }
    
    return false;
}

bool conexao_tftp() {
    std::string nomeArquivo, tipoRequisicao;
    bool cont = true;
    int conteudo[LIMITE_ARQUIVO];

    fd_cliente_tftp = socket(AF_INET, SOCK_STREAM, 0);
    // Verificação se o socket foi devidamente criado:
    if (fd_cliente == -1) {
        cerr << "Erro ao criar o socket!\n";
        return false;
    }

    while (cont)
    {
        cout << "Digite o tipo de requisicao (r/w): ";
        cin >> tipoRequisicao;
        cout << tipoRequisicao << '\n';
        if(tipoRequisicao.compare("quit") == 0) 
        {
            cout << "quit\n";
            return false;
        }
        if(tipoRequisicao.compare("r") == 0 || tipoRequisicao.compare("w") == 0) 
        {
            cont = false;
        }
    }

    cout << "Digite o nome do arquivo: ";
    cin >> nomeArquivo;
    bool sucesso = false;
    if(tipoRequisicao.compare("w") == 0)
    {
        sucesso = send_file(nomeArquivo);
        if(sucesso)
            std::cout << "Arquivo enviado com sucesso\n";
        else
            std::cout << "Falha ao enviar o arquivo\n";

    }
    if (tipoRequisicao.compare("r") == 0)
    {
        sucesso = get_file(nomeArquivo);
        if(sucesso)
            std::cout << "Arquivo baixado com sucesso\n";
        else
            std::cout << "Falha ao baixar o arquivo\n";
    }    
    return sucesso;
}


// Função auxiliar para divisão de strings
string split(string str, char delim){
    int i = 0, start = 0, end = int(str.size()) - 1;

    while(i++ < int(str.size())){
        if(str[i] == delim)
            start = i + 1;
    }
    string new_str = "";
    new_str.append(str, start, end - start + 1);

    return new_str;
}

// Função auxiliar para correção de caracter
void fix_string (char* arr, int length) {
    int i;
    for (i = 0; i < length; i++) { // trim \n
        if (arr[i] == '\n') {
            arr[i] = '\0';
            break;
        }
    }
}

// Tratamento do SIGINT
void ctrlc_handler(int s) {
    entrada = "/quit";
}

// Função representando a lógica de recebimento de mensagens
void *recv_thread(void *args){
    while (true) {  // Loop principal
        char mensagem_servidor[LIMITE_MENSAGEM];
        int recv_response = recv(
            fd_cliente,
            mensagem_servidor,
            sizeof(mensagem_servidor),
            MSG_NOSIGNAL
        );
        // Recebimento e impressão da mensagem recebida pelo servidor
        if (recv_response != -1) {
            cout << mensagem_servidor << "\n";
            if (strcmp(mensagem_servidor, "O adm te removeu deste canal :(\n") == 0) {
                entrada = "/quit";
                return NULL;
            }
        }
        // Tratamento do buffer
        memset(mensagem_servidor, 0, sizeof(mensagem_cliente));
    }
    return NULL;
}

// Função privada com a lógica da thread de envio de mensagens
void *send_thread(void *args){
    while (true) {
        cout << "Digite sua mensagem ou comando: \n";
        getline(cin, entrada);
        if (entrada == "/quit") {
            mandar_mensagem_servidor(entrada);
            return NULL;
        }
        else if (entrada == "/file") {
            mandar_mensagem_servidor(entrada);
            conexao_tftp();
        }
        else if (!mandar_mensagem_servidor(entrada))
            cerr << "Erro ao enviar a mensagem!\n";
    }
    return NULL;
}

int main() {

    // Atribuição de handler para SIGINT:
    signal(SIGINT, ctrlc_handler);

    // Criação do socket do cliente:
    fd_cliente = socket(AF_INET, SOCK_STREAM, 0);

    // Verificação se o socket foi devidamente criado:
    if (fd_cliente == -1) {
        cerr << "Erro ao criar o socket!\n";
        exit(-1);
    }
    cout << "Socket criado com sucesso!\n";

    // Leitura do comando /connect:
    entrada = "";
    while (entrada != "/connect") {
        cout << "Digite /connect para conectar-se ao servidor!\n";
        cin >> entrada; getchar();
    }

    // Conexão com o servidor:
    if (conectar_servidor(fd_cliente,endereco_servidor,config::SERVER_IP,config::SERVER_PORT))
        cout << "Conexão com servidor efetuada com sucesso\n";
    else
        cerr << "Erro ao conectar com o servidor!\n";

    // Leitura do nick e sua verificação:
    memset(nickname, 0, 50);
    while(strlen(nickname) <= 1){
        cout << "Bem vindo! Para comecar, digite um nickname: ";
        fgets(nickname, 50, stdin);
        fix_string(nickname, 50);
    }

    // Envio do nickname ao servidor:
    send(fd_cliente, nickname, 50, 0);

    // Leitura do canal e sua verificação:
    entrada = "";
    while (entrada.find("/join") == string::npos) {
        cout << "Digite /join <nomeDoCanal> para entrar em um canal!\nLembre-se que o padrão é: & ou # seguido de string sem espaços\n";
        getline(cin, entrada);
        if(entrada.find(',') != string::npos)
            entrada = "";
        if(entrada[6] == '&')   break;
        else if(entrada[6] == '#')  break;
        else if(entrada[6] != '&' && entrada[6] != '#')
            entrada = "";
        if(entrada.find_last_of(" ", 0) != 5)
            entrada = "";    
    }
    mandar_mensagem_servidor(entrada);

    // Criação da thread de recepção de mensagens:
    pthread_t input_thread;
    pthread_create(&input_thread, NULL, &recv_thread, NULL);

    // Criação da thread de envio de mensagens:
    pthread_t output_thread;
    pthread_create(&output_thread, NULL, &send_thread, NULL);

    bool shut_down = false;

    // Loop principal, para que as threads ocorram até o envio do comando quit
    while (!shut_down) {
        if (entrada == "/quit")
            shut_down = true;
        sleep(1);
    }

    // Finalização de ambas as threads
    pthread_detach(input_thread);
    pthread_detach(output_thread);

    close(fd_cliente);
    cout << "Socket fechado!\n";

    return 0;
}
