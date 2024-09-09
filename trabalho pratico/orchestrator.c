#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdlib.h> 
#include <stdio.h>
#include <string.h>
#include <sys/wait.h> 
#include <sys/time.h> 
#include <sys/types.h>

#define FIFO "fifoServerClient"
#define FIFO_SERVIDOR "fifo_servidor_"

// Função para converter um número inteiro numa string
void int_to_string(int num, char* str) {
    int i = 0;
    if (num == 0) {
        str[i++] = '0';
        str[i] = '\0';
        return;
    }
    while (num != 0) {
        int rem = num % 10;
        str[i++] = rem + '0';
        num = num / 10;
    }
    str[i] = '\0';

    // Inverte a string
    int start = 0;
    int end = i - 1;
    while (start < end) {
        char temp = str[start];
        str[start] = str[end];
        str[end] = temp;
        start++;
        end--;
    }
}

// Função para converter um número de ponto flutuante numa string
void double_to_string(double num, char* str) {
    int int_part = (int)num;
    double frac_part = num - int_part;

    int_to_string(int_part, str);
    strcat(str, ".");

    
    int frac_digits = 3; 
    char frac_str[5]; 
    int i = 0;
    while (frac_digits--) {
        frac_part *= 10;
        int digit = (int)frac_part;
        frac_str[i++] = digit + '0';
        frac_part -= digit;
    }
    frac_str[i] = '\0';
    strcat(str, frac_str);
}

// Função para criar a mensagem formatada
void create_message(int numero_pedidos, char* buffer, double tempo, char* mensagem) {
    char num_str[20]; 
    char tempo_str[20]; 

    int_to_string(numero_pedidos, num_str);
    double_to_string(tempo, tempo_str);

    // Construir a mensagem formatada
    strcpy(mensagem, "A tarefa ");
    strcat(mensagem, num_str);
    strcat(mensagem, " com o pedido ");
    strcat(mensagem, buffer + 10); 
    strcat(mensagem, " foi executada em ");
    strcat(mensagem, tempo_str);
    strcat(mensagem, " ms");
    mensagem[strlen(mensagem)] = '\0'; 
}

// Função para mover o resultado para a pasta do cliente
void move_resultado(char* filename, char cliente_id) {
    char pasta_cliente[20];
    int_to_string(cliente_id, pasta_cliente); 

    // Criar a pasta do cliente se não existir
    mkdir(pasta_cliente, 0777);

    char caminho_completo[50];
    strcpy(caminho_completo, pasta_cliente);
    strcat(caminho_completo, "/");
    strcat(caminho_completo, filename);

    // Renomear o arquivo para movê-lo para a pasta do cliente
    rename(filename, caminho_completo);
}

int mysystem(char* command, int numero_pedido, int fd_log, char cliente_id) {
    // Cria um processo filho
    pid_t pid = fork();
    if (pid == -1) {
        perror("Erro ao criar processo filho");
        return -1;
    } else if (pid == 0) { 
        // Escreve no arquivo log sobre o comando a ser executado
        write(fd_log, "Comando a ser executado: ", 26);
        write(fd_log, command, strlen(command));
        write(fd_log, "\n", 1);


        char output_filename[20];
        output_filename[0] = '\0'; 

        int temp = numero_pedido;
        while (temp > 0) {
            char digit = '0' + (temp % 10); 
            memmove(output_filename + 1, output_filename, strlen(output_filename) + 1); 
            output_filename[0] = digit; 
            temp /= 10; 
        }

        strcat(output_filename, ".txt"); 
        output_filename[strlen(output_filename)] = '\0'; 
        int fd_output = open(output_filename, O_WRONLY | O_CREAT | O_TRUNC, 0660);
        if (fd_output == -1) {
            perror("Erro ao abrir arquivo de saída");
            return -1;
        }
        move_resultado(output_filename, cliente_id);

        // Redireciona a saída padrão para o arquivo
        if (dup2(fd_output, STDOUT_FILENO) == -1) {
            perror("Erro ao redirecionar a saída padrão");
            return -1;
        }

        // Executa o comando
        int exec_ret = execl("/bin/sh", "sh", "-c", command, NULL);
        if (exec_ret == -1) {
            perror("Erro ao executar o comando");
            return -1;
        }
        close(fd_output);


        exit(0); 
    } else { 
        wait(NULL);
    }

    return 0;
}


int mystatus(char* command, int fd_cliente) {
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("Erro ao criar o pipe");
        return -1;
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("Erro ao criar o processo filho");
        return -1;
    } else if (pid == 0) { 
        // Fecha a leitura do pipe
        close(pipefd[0]);

        // Redireciona a saída padrão para o FIFO do cliente
        if (dup2(pipefd[1], STDOUT_FILENO) == -1) {
            perror("Erro ao redirecionar a saída padrão");
            return -1;
        }

        // Executa o comando
        int exec_ret = execl("/bin/sh", "sh", "-c", command, NULL);
        if (exec_ret == -1) {
            perror("Erro ao executar o comando");
            return -1;
        }
    } else { 
        // Fecha a escrita do pipe
        close(pipefd[1]);

        char buffer[1024];
        ssize_t bytes_read;
        while ((bytes_read = read(pipefd[0], buffer, sizeof(buffer))) > 0) {
            write(fd_cliente, buffer, bytes_read);
        }
        wait(NULL);
    }

    return 0;
}


// Função para obter o tempo decorrido
double get_elapsed_time(struct timeval start_time) {
    struct timeval end_time;
    gettimeofday(&end_time, NULL);
    return (double)(end_time.tv_sec - start_time.tv_sec) * 1000.0 + (double)(end_time.tv_usec - start_time.tv_usec) / 1000.0;
}

int main() {
    int fd_fifo, fd_log, fd_servidor;
    struct stat stats;
    int numero_pedidos = 0; 

    // Remover FIFO existente, se houver
    if (stat(FIFO, &stats) == 0)
        if (unlink(FIFO) < 0) {
            perror("Unlinked failed");
            return -1;
        }

    // Criar FIFO para cliente
    if (mkfifo(FIFO, 0660) == -1) {
        perror("Erro a criar FIFO");
        return -1;
    }

    // Abrir FIFO do cliente
    if ((fd_fifo = open(FIFO, O_RDONLY, 0660)) == -1) {
        perror("Erro a abrir FIFO");
        return -1;
    }

    // Verificar se o arquivo de log já existe
    if (stat("log.txt", &stats) != -1) {
        // Se o arquivo de log já existir, abre para escrita 
        fd_log = open("log.txt", O_WRONLY | O_APPEND, 0660);
    } else {
        // Se o arquivo de log não existir, cria-o
        fd_log = open("log.txt", O_WRONLY | O_CREAT | O_TRUNC, 0660);
    }
    if (fd_log == -1) {
        perror("Erro a abrir logs");
        return -1;
    }

    // Buffer e variáveis para leitura
    int buffer_size = 300;
    char buffer[buffer_size];
    int read_bytes;

    while (1) {
        // Leitura do FIFO do cliente
        read_bytes = read(fd_fifo, buffer, buffer_size);
        if (read_bytes >= 0) {
            // Remoção do caracter de nova linha, do final da mensagem
            if (buffer[read_bytes - 1] == '\n') {
                buffer[read_bytes - 1] = '\0';
            }
            buffer[read_bytes] = '\0';

            // Construção do nome do FIFO do servidor, com base no primeiro caracter do buffer
            char fifo_servidor[20] = "fifo_servidor_";
            fifo_servidor[14] = buffer[0];
            fifo_servidor[15] = '\0';

            // Abrir o FIFO do servidor para escrita
            int fd_servidor = open(fifo_servidor, O_WRONLY, 0660);
            if (fd_servidor == -1) {
                continue;
            }

            // Execução do comando "status"
            if (strncmp(buffer + 2, "status", 6) == 0) {
                pid_t status_pid = fork();
                if (status_pid == 0) {
                    mystatus("cat log.txt", fd_servidor);
                    exit(0); // Termina o processo filho após a execução
                }
            } else {
                numero_pedidos++;

            
                pid_t execute_pid = fork();
                if (execute_pid == -1) {
                    perror("Erro ao criar processo filho para execute");
                } else if (execute_pid == 0) {
                    write(fd_log, buffer, strlen(buffer));
                    write(fd_log, "\n", 1);

                    // Construir a mensagem com o número de pedidos e o comando
                    char mensagem[200];
                    double tempo;
                    struct timeval start_time;
                    gettimeofday(&start_time, NULL); 
                    mysystem(buffer + 10, numero_pedidos, fd_log, atoi(&buffer[0]));
                    tempo = get_elapsed_time(start_time); // Calcula o tempo de execução
                    create_message(numero_pedidos, buffer, tempo, mensagem); 
                    int len = strlen(mensagem);

                    write(fd_log, mensagem, len);
                    write(fd_log, "\n", 1);

                    char mensagem1[100];
                    int len1 = 0;

                    // Construir a mensagem de sucesso
                    strcpy(mensagem1, "Tarefa ");
                    char num_str[20];
                    int_to_string(numero_pedidos, num_str); 
                    strcat(mensagem1, num_str);
                    strcat(mensagem1, " concluída com sucesso\n");
                    len1 = strlen(mensagem1);
                    write(fd_servidor, mensagem1, len1);
                    write(fd_fifo, mensagem, len);
                    exit(0); 
                }
            }
            close(fd_servidor); 
        }
    }
    close(fd_log);
    close(fd_servidor);
    return 0;
}
