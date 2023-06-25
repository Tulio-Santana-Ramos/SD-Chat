# Trabalho Final de Redes de Computadores
Repositório destinado ao trabalho final da disciplina de Redes de Computadores 2022-01

## Integrantes:
- 11796531 &nbsp; Israel Felipe da Silva
- 11796893 &nbsp; Luiz Fernando Rabelo
- 11795526 &nbsp; Tulio Santana Ramos

### Confecção
Foram desenvolvidas 2 aplicações representando ambos os lados, Servidor ([Servidor.cpp](https://github.com/Tulio-Santana-Ramos/TrabalhoFinalRedes/blob/main/Servidor.cpp)) e Cliente ([Cliente.cpp](https://github.com/Tulio-Santana-Ramos/TrabalhoFinalRedes/blob/main/Cliente.cpp)), de uma simples comunicação. Através do uso de diferentes terminais, múltiplos clientes poderão se conectar ao mesmo servidor e conversar entre si, contando que estejam no mesmo canal de envio de mensagens.
O projeto foi desenvolvido em sistemas Linux (Ubuntu 22.04 LTS) e Windows 11 em conjunto com WSL 2.0 (Ubuntu 22.04 LTS) através da linguagem C++ e suas bibliotecas padrões.

### Compilação
Para fácil compilação dos programas, utiliza-se o [Makefile](https://github.com/Tulio-Santana-Ramos/TrabalhoFinalRedes/blob/main/Makefile) presente neste repositório. A diretiva `make` compila tanto o código do cliente quanto o código do servidor.

### Execução
A execução de ambas as aplicações também fica por parte do [Makefile](https://github.com/Tulio-Santana-Ramos/TrabalhoFinalRedes/blob/main/Makefile) através das respectivas tags de cada uma (`make srun` executa o servidor e `make crun` executa o cliente). Uma vez que ambas as aplicações tenham sido iniciadas e a conexão seja estabelecida, pode-se trocar mensagens de até 4096 caracteres. Mensagens com um número maior de caracteres serão divididas e enviadas em mensagens distintas. Para excluir os arquivos executáveis, basta utilizar `make clean`.

### Comandos
#### Disponíveis para todo usuário:
- /connect tenta efetuar a conexão entre o cliente e o servidor configurado. Pode ser efetuado na mesma máquina ou em máquinas diferentes na mesma rede (alterando o IP do servidor dentro do arquivo de Cliente).
- /join <nomedoCanal> permite que o usuário se conecte a algum canal existente ou crie um novo canal neste servidor, neste caso também se tornando o administrador do mesmo. O nome dos canais seguem as restrições apresentadas no [RFC-1459](https://datatracker.ietf.org/doc/html/rfc1459#section-1.3).
- /nickname <novoNick> permite que o usuário altere seu nickname original. Este comando pode ser executado diferentes vezes enquanto conectado em um canal. Os nicks devem possuir até 50 caracteres.
- /ping é utilizado para verificar o envio de mensagens por ambas as aplicações, fazendo com que, ao receber este comando, o Servidor envie uma nova mensagem ('pong!') para o Cliente conectado.
- /quit fecha o socket da conexão, finalizando portanto a execução de ambas as aplicações.
#### Disponíveis para administradores de canais:
- /kick <nickUsuario> fecha, de maneira forçada, a conexão de algum usuário presente no mesmo canal do administrador, não permite auto exclusão.
- /mute <nickUsuario> muta usuário, ou seja, impede que o usuário selecionado envie mensagens neste canal.
- /unmute <nickUsuario> desmuta usuário, ou seja, permite que ele envie mensagens novamente neste canal. Não há alteração de comportamento caso o usuário selecionado não tenha sido mutado previamente.
- /whois <nickUsuario> permite que somente o administrador do canal receba o endereço IP do usuário selecionado.

### Execução em Vídeo
Para demonstração do código, pode-se acessar este [link]() em que os integrantes do grupo apresentam as diferentes funcionalidades de ambas as partes desta aplicação e comentam sobre o a confecção e execução do código.