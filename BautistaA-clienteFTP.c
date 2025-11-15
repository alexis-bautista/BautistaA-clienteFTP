/* BautistaA-clienteFTP.c - Cliente FTP concurrente*/
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>

extern int errno;
int connectsock(const char *host, const char *service, const char *transport);
int errexit(const char *format, ...);
int connectTCP(const char *host, const char *service);

#define LINELEN 512
#define BUFSIZE 8192
#define MAX_CONCURRENT 5

/* Estructura para manejo de session FTP*/
typedef struct{
    int ctrl_sock;
    char server_ip[64];
    int data_port;
    int passive_mode;
    int logged_in;
    pid_t active_transfers[MAX_CONCURRENT];
    int transfer_count;
} FTPSession;

/* Prototipos de funciones */
int TCPftp(FTPSession *session);
int ftp_read_response(int sock, char *buffer, int size);
int ftp_send_command(int sock, const char *cmd);
int ftp_login(FTPSession *session, const char *user, const char *pass);
int port_command(FTPSession *session);
int ftp_pasv_command(FTPSession *session, char *server_ip, int *port);
int ftp_stor(FTPSession *session, const char *local_file, const char *remote_file);
int ftp_retr(FTPSession *session, const char *remote_file, const char *local_file);
int ftp_mkd(FTPSession *session, const char *dirname);
int ftp_pwd(FTPSession *session);
int ftp_dele(FTPSession *session, const char *filename);
int ftp_rest(FTPSession *session, long offset);
void handle_sigchld(int sig);
void print_help();
int add_transfer_process(FTPSession *session, pid_t pid);
void cleanup_finished_transfers(FTPSession *session);

/*-----------------------------------------------
* main - Cliente FTP concurrente
*-----------------------------------------------*/

int main(int argc, char *argv[]){
    FTPSession session;
    char host[128];
    char service[32];

    /* Configurar manejador de senales para procesos hijos*/
    signal(SIGCHLD, handle_sigchld);

    /* Validar argumentos*/
    if (argc != 3){
        fprintf(stderr, "Uso: %s <host> <puerto>\n", argv[0]);
        fprintf(stderr, "Ejemplo: %s ftp.example.com 21\n", argv[0]);
        exit(1);
    }

    strncpy(host, argv[1], sizeof(host) - 1);
    strncpy(service, argv[2], sizeof(service) - 1);

    /* Inicializar session*/
    memset(&session, 0, sizeof(FTPSession));
    session.passive_mode = 1; /* Por defecto usar modo pasivo*/
    session.transfer_count = 0;

    /* Conectar al servidor FTP*/
    printf("Conectando a %s:%s...\n", host, service);
    session.ctrl_sock = connectTCP(host, service);
    strncpy(session.server_ip, host, sizeof(session.server_ip) - 1);

    char buffer[LINELEN];
    ftp_read_response(session.ctrl_sock, buffer, LINELEN);
    printf("%s", buffer);

    /*Ejecutar cliente FTP*/
    TCPftp(&session);

    /* Cerrar conexion*/
    close(session.ctrl_sock);
    return 0;
}

/*-------------------------------
*TCPftp - Manejo de comandos FTP 
*-------------------------------
*/
int TCPftp(FTPSession *session) {
    char input[LINELEN];
    char cmd[32], arg1[256], arg2[256];
    int running = 1;
    
    printf("\nCliente FTP listo. Escriba 'help' para ver comandos disponibles.\n");
    
    while (running) {
        /* Limpiar transferencias finalizadas */
        cleanup_finished_transfers(session);
        
        printf("ftp> ");
        fflush(stdout);
        
        if (fgets(input, LINELEN, stdin) == NULL)
            break;
            
        /* Eliminar salto de linea */
        input[strcspn(input, "\n")] = 0;
        
        /* Parsear comando */
        memset(cmd, 0, sizeof(cmd));
        memset(arg1, 0, sizeof(arg1));
        memset(arg2, 0, sizeof(arg2));
        
        sscanf(input, "%s %s %s", cmd, arg1, arg2);
        
        if (strlen(cmd) == 0)
            continue;
            
        /* Procesar comandos */
        if (strcmp(cmd, "help") == 0) {
            print_help();
        }
        else if (strcmp(cmd, "user") == 0) {
            if (strlen(arg1) == 0) {
                printf("Uso: user <nombre_usuario>\n");
                continue;
            }
            char buffer[LINELEN];
            sprintf(buffer, "USER %s\r\n", arg1);
            ftp_send_command(session->ctrl_sock, buffer);
            ftp_read_response(session->ctrl_sock, buffer, LINELEN);
            printf("%s", buffer);
        }
        else if (strcmp(cmd, "pass") == 0) {
            if (strlen(arg1) == 0) {
                printf("Uso: pass <contraseña>\n");
                continue;
            }
            char buffer[LINELEN];
            sprintf(buffer, "PASS %s\r\n", arg1);
            ftp_send_command(session->ctrl_sock, buffer);
            ftp_read_response(session->ctrl_sock, buffer, LINELEN);
            printf("%s", buffer);
            if (strncmp(buffer, "230", 3) == 0)
                session->logged_in = 1;
        }
        else if (strcmp(cmd, "login") == 0) {
            if (strlen(arg1) == 0 || strlen(arg2) == 0) {
                printf("Uso: login <usuario> <contraseña>\n");
                continue;
            }
            ftp_login(session, arg1, arg2);
        }
        else if (strcmp(cmd, "stor") == 0 || strcmp(cmd, "put") == 0) {
            if (strlen(arg1) == 0) {
                printf("Uso: stor <archivo_local> [archivo_remoto]\n");
                continue;
            }
            const char *remote = (strlen(arg2) > 0) ? arg2 : arg1;
            ftp_stor(session, arg1, remote);
        }
        else if (strcmp(cmd, "retr") == 0 || strcmp(cmd, "get") == 0) {
            if (strlen(arg1) == 0) {
                printf("Uso: retr <archivo_remoto> [archivo_local]\n");
                continue;
            }
            const char *local = (strlen(arg2) > 0) ? arg2 : arg1;
            ftp_retr(session, arg1, local);
        }
        else if (strcmp(cmd, "pwd") == 0) {
            ftp_pwd(session);
        }
        else if (strcmp(cmd, "mkd") == 0 || strcmp(cmd, "mkdir") == 0) {
            if (strlen(arg1) == 0) {
                printf("Uso: mkd <nombre_directorio>\n");
                continue;
            }
            ftp_mkd(session, arg1);
        }
        else if (strcmp(cmd, "dele") == 0 || strcmp(cmd, "delete") == 0) {
            if (strlen(arg1) == 0) {
                printf("Uso: dele <archivo>\n");
                continue;
            }
            ftp_dele(session, arg1);
        }
        else if (strcmp(cmd, "passive") == 0) {
            session->passive_mode = !session->passive_mode;
            printf("Modo pasivo: %s\n", session->passive_mode ? "ON" : "OFF");
        }
        else if (strcmp(cmd, "status") == 0) {
            printf("Transferencias activas: %d/%d\n", 
                   session->transfer_count, MAX_CONCURRENT);
        }
        else if (strcmp(cmd, "quit") == 0 || strcmp(cmd, "exit") == 0) {
            ftp_send_command(session->ctrl_sock, "QUIT\r\n");
            char buffer[LINELEN];
            ftp_read_response(session->ctrl_sock, buffer, LINELEN);
            printf("%s", buffer);
            running = 0;
        }
        else {
            printf("Comando desconocido: %s\n", cmd);
            printf("Escriba 'help' para ver comandos disponibles.\n");
        }
    }
    
    return 0;
}

/* ----------------------------------------------------- 
* ftp_read_response  -  Lee la respuesta del servidor
*-------------------------------------------------------
*/

int ftp_read_response(int sock, char *buffer, int size){
    int n;
    memset(buffer, 0, size);

    /* leer respuestas linea por linea*/
    n = recv(sock, buffer, size - 1, 0);
    if (n<0 ){
        return -1;
    }
    return n;
}

/*-------------------------------------------- -
* ftp_send_command - Envia comando al servidor
*----------------------------------------------
*/
int ftp_send_command(int sock, const char *cmd){
    int n = send(sock, cmd, strlen(cmd), 0);
    return n;
}

/* ---------------------------------------
* ftp_login - Inicia sesion en el servidor
*-----------------------------------------
*/
int ftp_login(FTPSession *session, const char *user, const char *pass){
    char buffer [LINELEN];

    /* Enviar USER */
    sprintf(buffer, "USER %s\r\n", user);
    ftp_send_command(session->ctrl_sock, buffer);
    ftp_read_response(session->ctrl_sock, buffer, LINELEN);
    printf("%s", buffer);

    /* Enviar PASS */
    sprintf(buffer, "PASS %s\r\n", pass);
    ftp_send_command(session->ctrl_sock, buffer);
    ftp_read_response(session->ctrl_sock, buffer, LINELEN);
    printf("%s", buffer);

    if (strncmp(buffer, "230", 3) == 0){
        session->logged_in = 1;
        return 0;
    }
    return -1;
}

/*--------------------------------------
*ftp_pasv_command - Entra en modo pasivo
*---------------------------------------
 */
int ftp_pasv_command(FTPSession *session, char *server_ip, int *port){
    char buffer[LINELEN];

    ftp_send_command(session->ctrl_sock, "PASV\r\n");
    ftp_read_response(session->ctrl_sock, buffer, LINELEN);
    printf("%s", buffer);

    /*parsear respuesta PASV (227 Entering Passive Mode (h1,h2,h3,h4,p1,p2))*/
    if (strncmp(buffer, "227", 3) == 0) {
        int h1, h2, h3, h4, p1, p2;
        char *start = strchr(buffer, '(');
        if (start && sscanf(start, "(%d,%d,%d,%d,%d,%d)", &h1, &h2, &h3, &h4, &p1, &p2) == 6) {
            sprintf(server_ip, "%d.%d.%d.%d", h1, h2, h3, h4);
            *port = p1 * 256 + p2;
            return 0;
        }
    }
    return -1;
}

/*---------------------------------------------------------
* ftp_stor - Sube un archivo al servidor (modo concurrente)
*----------------------------------------------------------
*/
int ftp_stor(FTPSession *session, const char *local_file, const char *remote_file) {
    if (session->transfer_count >= MAX_CONCURRENT) {
        printf("Error: Máximo de transferencias concurrentes alcanzado (%d)\n", MAX_CONCURRENT);
        return -1;
    }
    
    pid_t pid = fork();
    
    if (pid < 0) {
        perror("fork");
        return -1;
    }
    
    if (pid == 0) {
        /* Proceso hijo - realiza la transferencia */
        FILE *fp = fopen(local_file, "rb");
        if (!fp) {
            fprintf(stderr, "[PID %d] Error: No se puede abrir %s\n", getpid(), local_file);
            exit(1);
        }
        
        /* Establecer conexión de datos */
        int data_sock;
        if (session->passive_mode) {
            char ip[64];
            int port;
            if (ftp_pasv_command(session, ip, &port) < 0) {
                fclose(fp);
                exit(1);
            }
            
            char port_str[16];
            sprintf(port_str, "%d", port);
            data_sock = connectTCP(ip, port_str);
        }
        
        /* Enviar comando STOR */
        char buffer[LINELEN];
        sprintf(buffer, "STOR %s\r\n", remote_file);
        ftp_send_command(session->ctrl_sock, buffer);
        ftp_read_response(session->ctrl_sock, buffer, LINELEN);
        printf("[PID %d] %s", getpid(), buffer);
        
        /* Transferir datos */
        char data[BUFSIZE];
        size_t bytes;
        long total = 0;
        
        while ((bytes = fread(data, 1, BUFSIZE, fp)) > 0) {
            send(data_sock, data, bytes, 0);
            total += bytes;
        }
        
        fclose(fp);
        close(data_sock);
        
        /* Leer respuesta final */
        ftp_read_response(session->ctrl_sock, buffer, LINELEN);
        printf("[PID %d] %s", getpid(), buffer);
        printf("[PID %d] Transferencia completada: %ld bytes\n", getpid(), total);
        
        exit(0);
    }
    
    /* Proceso padre - registrar transferencia */
    add_transfer_process(session, pid);
    printf("Transferencia iniciada (PID %d): %s -> %s\n", pid, local_file, remote_file);
    
    return 0;
}

/*---------------------------------------------------------------
* ftp_retr - Descargar un archivo del servidor (modo concurrente)
*----------------------------------------------------------------
*/
int ftp_retr(FTPSession *session, const char *remote_file, const char *local_file) {
    if (session->transfer_count >= MAX_CONCURRENT) {
        printf("Error: Máximo de transferencias concurrentes alcanzado (%d)\n", MAX_CONCURRENT);
        return -1;
    }
    
    pid_t pid = fork();
    
    if (pid < 0) {
        perror("fork");
        return -1;
    }
    
    if (pid == 0) {
        /* Proceso hijo */
        FILE *fp = fopen(local_file, "wb");
        if (!fp) {
            fprintf(stderr, "[PID %d] Error: No se puede crear %s\n", getpid(), local_file);
            exit(1);
        }
        
        /* Establecer conexión de datos */
        int data_sock;
        if (session->passive_mode) {
            char ip[64];
            int port;
            if (ftp_pasv_command(session, ip, &port) < 0) {
                fclose(fp);
                exit(1);
            }
            
            char port_str[16];
            sprintf(port_str, "%d", port);
            data_sock = connectTCP(ip, port_str);
        }
        
        /* Enviar comando RETR */
        char buffer[LINELEN];
        sprintf(buffer, "RETR %s\r\n", remote_file);
        ftp_send_command(session->ctrl_sock, buffer);
        ftp_read_response(session->ctrl_sock, buffer, LINELEN);
        printf("[PID %d] %s", getpid(), buffer);
        
        /* Recibir datos */
        char data[BUFSIZE];
        ssize_t bytes;
        long total = 0;
        
        while ((bytes = recv(data_sock, data, BUFSIZE, 0)) > 0) {
            fwrite(data, 1, bytes, fp);
            total += bytes;
        }
        
        fclose(fp);
        close(data_sock);
        
        /* Leer respuesta final */
        ftp_read_response(session->ctrl_sock, buffer, LINELEN);
        printf("[PID %d] %s", getpid(), buffer);
        printf("[PID %d] Descarga completada: %ld bytes\n", getpid(), total);
        
        exit(0);
    }
    
    /* Proceso padre */
    add_transfer_process(session, pid);
    printf("Descarga iniciada (PID %d): %s -> %s\n", pid, remote_file, local_file);
    
    return 0;
}

/*---------------------------------------
* ftp_pwd - Muestra el directorio actual
*----------------------------------------
*/
int ftp_pwd(FTPSession *session){
    char buffer[LINELEN];
    ftp_send_command(session->ctrl_sock, "PWD\r\n");
    ftp_read_response(session->ctrl_sock, buffer, LINELEN);
    printf("%s", buffer);
    return 0;
}

/*----------------------------
* ftp_mkd - Crea un directorio
*-----------------------------
*/
int ftp_mkd(FTPSession *session, const char *dirname) {
    char buffer[LINELEN];
    sprintf(buffer, "MKD %s\r\n", dirname);
    ftp_send_command(session->ctrl_sock, buffer);
    ftp_read_response(session->ctrl_sock, buffer, LINELEN);
    printf("%s", buffer);
    return 0;
}

/*------------------------------
* ftp_dele - Elimina un archivo
*-------------------------------
*/
int ftp_dele(FTPSession *session, const char *filename) {
    char buffer[LINELEN];
    sprintf(buffer, "DELE %s\r\n", filename);
    ftp_send_command(session->ctrl_sock, buffer);
    ftp_read_response(session->ctrl_sock, buffer, LINELEN);
    printf("%s", buffer);
    return 0;
}

/*----------------------------------------------------------
 * ftp_rest - Establece punto de reinicio para transferencia
 *----------------------------------------------------------
 */
int ftp_rest(FTPSession *session, long offset) {
    char buffer[LINELEN];
    sprintf(buffer, "REST %ld\r\n", offset);
    ftp_send_command(session->ctrl_sock, buffer);
    ftp_read_response(session->ctrl_sock, buffer, LINELEN);
    printf("%s", buffer);
    return 0;
}

/*-------------------------------------------------------------------
 * add_transfer_process - Agrega proceso de transferencia al registro
 *-------------------------------------------------------------------
 */
int add_transfer_process(FTPSession *session, pid_t pid) {
    if (session->transfer_count >= MAX_CONCURRENT)
        return -1;
    
    session->active_transfers[session->transfer_count++] = pid;
    return 0;
}

/*------------------------------------------------------------------------
 * cleanup_finished_transfers - Limpia procesos terminados
 *------------------------------------------------------------------------
*/

void cleanup_finished_transfers(FTPSession *session) {
    int status;
    pid_t pid;
    int cleaned = 0;
    
    /* Verificar cada proceso en la lista */
    for (int i = 0; i < session->transfer_count; ) {
        pid = waitpid(session->active_transfers[i], &status, WNOHANG);
        
        if (pid > 0) {
            /* Proceso termino, removerlo */
            cleaned++;
            /* Desplazar elementos */
            for (int j = i; j < session->transfer_count - 1; j++) {
                session->active_transfers[j] = session->active_transfers[j + 1];
            }
            session->active_transfers[session->transfer_count - 1] = 0;
            session->transfer_count--;
            /* No incrementar i porque desplazamos elementos */
        } else if (pid == 0) {
            /* Proceso aún corriendo */
            i++;
        } else {
            /* Error o proceso no existe, removerlo tambien */
            for (int j = i; j < session->transfer_count - 1; j++) {
                session->active_transfers[j] = session->active_transfers[j + 1];
            }
            session->active_transfers[session->transfer_count - 1] = 0;
            session->transfer_count--;
        }
    }
    
    if (cleaned > 0) {
        printf("Transferencias completadas: %d\n", cleaned);
    }
}

/*--------------------------------------------------------
 * handle_sigchld - Manejador de senal para procesos hijo
 *--------------------------------------------------------
 */
void handle_sigchld(int sig) {
    int saved_errno = errno;
    while (waitpid(-1, NULL, WNOHANG) > 0);
    errno = saved_errno;
}

/*---------------------------------------
 * print_help - Muestra ayuda de comandos
 *---------------------------------------
 */
 void print_help() {
    printf("\nComandos disponibles:\n");
    printf("  login <user> <pass>     - Iniciar sesion\n");
    printf("  user <username>         - Enviar nombre de usuario\n");
    printf("  pass <password>         - Enviar contraseña\n");
    printf("  stor <local> [remote]   - Subir archivo\n");
    printf("  put <local> [remote]    - Alias de stor\n");
    printf("  retr <remote> [local]   - Descargar archivo\n");
    printf("  get <remote> [local]    - Alias de retr\n");
    printf("  pwd                     - Mostrar directorio actual\n");
    printf("  mkd <dirname>           - Crear directorio\n");
    printf("  dele <filename>         - Eliminar archivo\n");
    printf("  passive                 - Alternar modo pasivo\n");
    printf("  status                  - Ver transferencias activas\n");
    printf("  help                    - Mostrar esta ayuda\n");
    printf("  quit                    - Salir\n");
    printf("\nNota: Soporta hasta %d transferencias concurrentes\n", MAX_CONCURRENT);
}