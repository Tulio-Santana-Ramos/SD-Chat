#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <bits/stdc++.h>

#define LIMITE_MENSAGEM 4097    // Definição do limite do tamanho da mensagem

using namespace std;

int fd_cliente; // Número de identificação do cliente
string entrada; // Entrada padrão
char nickname[50];  // Nickname do usuário
sockaddr_in endereco_servidor;
char mensagem_cliente[LIMITE_MENSAGEM];
char mensagem_servidor[LIMITE_MENSAGEM];

// Função para conexão do servidor
bool conectar_servidor() {
    // Configuração da porta e IP para o mesmo endereço do servidor:
    endereco_servidor.sin_family = AF_INET;
    endereco_servidor.sin_port = htons(2000);
    // O endereço IP abaixo pode ser setado para o local, caso rode o servidor e esta aplicação na mesma máquina
    // Assim como pode ser inserido um IP distinto, caso outra máquina na mesma rede esteja executando a aplicação de servidor
    endereco_servidor.sin_addr.s_addr = inet_addr("127.0.0.1");
    // Retorno da conexão com o servidor:
    return connect(fd_cliente, (sockaddr *) &endereco_servidor, sizeof(endereco_servidor)) != -1;
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
        // cout << "Digite sua mensagem ou comando: \n";
        getline(cin, entrada);
        if (entrada == "/quit") {
            mandar_mensagem_servidor(entrada);
            return NULL;
        }
        if (!mandar_mensagem_servidor(entrada))
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
    if (conectar_servidor())
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
