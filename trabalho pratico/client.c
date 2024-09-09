#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h> 
#include <string.h> 
#include <sys/stat.h>
#include <sys/types.h> 


// Função para converter um número inteiro numa string
void int_to_string(int num, char *str) {
    char temp[20]; 
    int i = 0;
    int sign = 0;

    // Verifica se o número é negativo
    if (num < 0) {
        sign = 1;
        num = -num;
    }
    do {
        temp[i++] = num % 10 + '0';
        num /= 10;
    } while (num > 0);

    // Adiciona o sinal, se necessário
    if (sign) {
        temp[i++] = '-';
    }

    int j = 0;
    while (i > 0) {
        str[j++] = temp[--i];
    }

    str[j] = '\0';
}


int main(int argc, char *argv[]) {
    // Verifica se o número de argumentos está correto
    if (argc != 4) {
        perror("Uso incorreto. Use: <id> <tempoestimado> <comando>");
        return -1;
    }

    // Obtém o id do cliente 
    int id = atoi(argv[1]);
    // Obtém o tempo estimado 
    int tempoestimado = atoi(argv[2]);

    // Cria o nome do FIFO do servidor com base no id fornecido
    char fifo_servidor[20] = "fifo_servidor_";
    char id_str[10];
    int_to_string(id, id_str);
    strcat(fifo_servidor, id_str);

    int fd;
    struct stat stats;

    // Abre o FIFO do cliente para escrita
    if ((fd = open("fifoServerClient", O_WRONLY, 0660)) == -1) {
        perror("Erro ao abrir FIFO do cliente");
        return -1;
    }

    int folder_exists = open(id_str, O_RDONLY | O_DIRECTORY);
    if (folder_exists == -1) {
        if (mkdir(id_str, 0777) == -1) {
            perror("Erro ao criar a pasta do cliente");
            return -1;
        }
    }

    // Cria o FIFO do servidor com base no ID fornecido
    if (stat(fifo_servidor, &stats) == 0)
        if (unlink(fifo_servidor) < 0) {
            perror("Unlinked failed");
            return -1;
        }

    if (mkfifo(fifo_servidor, 0660) == -1) {
        perror("Erro a criar FIFO servidor");
        return -1;
    }

    // Constrói o comando completo com o ID e o comando fornecido
    char comando_com_id[50];
    int_to_string(id, id_str);
    strcpy(comando_com_id, id_str);
    strcat(comando_com_id, " ");
    strcat(comando_com_id, argv[3]);

    // Escreve o comando completo diretamente no FIFO do cliente
    if (write(fd, comando_com_id, strlen(comando_com_id)) == -1) {
        perror("Erro ao escrever no FIFO do cliente");
        return -1;
    }

    // Leitura do FIFO do servidor, para ser possível saber se o comando foi executado 
    int fd_servidor;
    if ((fd_servidor = open(fifo_servidor, O_RDONLY)) == -1) {
        perror("Erro ao abrir FIFO do servidor para leitura");
        return -1;
    }

    char buffer_servidor[10000];
    int read_bytes_servidor = read(fd_servidor, buffer_servidor, 10000);
    if (read_bytes_servidor > 0) {
        if (write(STDOUT_FILENO, buffer_servidor, read_bytes_servidor) == -1) {
            perror("Erro ao escrever no STDOUT");
            return -1;
        }
    } else {
        perror("Erro ao ler do FIFO do servidor");
        return -1;
    }
    close(fd_servidor);

    close(fd);
    return 0;
}
