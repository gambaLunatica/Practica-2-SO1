#define _POSIX_C_SOURCE 200112L
// librerias a usar
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>
#include <linux/limits.h>
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/stat.h>

#include <fcntl.h>

// definir promp
#define PROMPT '$'

// definir colores
#define RESET "\033[0m"
#define NEGRO_T "\x1b[30m"
#define NEGRO_F "\x1b[40m"
#define GRIS_T "\x1b[94m"
#define ROJO_T "\x1b[31m"
#define VERDE_T "\x1b[32m"
#define AMARILLO_T "\x1b[33m"
#define AZUL_T "\x1b[34m"
#define MAGENTA_T "\x1b[35m"
#define CYAN_T "\x1b[36m"
#define BLANCO_T "\x1b[97m"
#define NEGRITA "\x1b[1m"

// Debugs
#define DEBUGN1 1

// definición funciones
void imprimir_prompt();
char *read_line(char *line);
int execute_line(char *line);
int parse_args(char **args, char *line);
int check_internal(char **args);
int internal_cd(char **args);
int internal_export(char **args);
int internal_source(char **args);
int internal_jobs(char **args);
int internal_fg(char **args);
int internal_bg(char **args);
char *eliminar_chars(char *str);
void ctrlc(int signum);
void ctrlz(int signum);
void reaper(int signum);
int is_background(char **args);
int jobs_list_add(pid_t pid, char estado, char *cmd);
int jobs_lsit_find(pid_t pid);
int jobs_list_remove(int pos);
int is_output_redirection(char **args);

// Variable para leer una line
#define COMMAND_LINE_SIZE 1024
#define ARGS_SIZE 64
#define N_JOBS 10

// Definición de la estructura info_job
struct info_job
{
    pid_t pid;
    char estado;                 // ‘N’: Ninguno, ‘E’: Ejecutándose, ‘D’: Detenido, ‘F’: Finalizado
    char cmd[COMMAND_LINE_SIZE]; // Línea de comando asociada
};
// Declaración de jobs_list y mi_shell
static struct info_job jobs_list[N_JOBS];
char mi_shell[COMMAND_LINE_SIZE];

int static n_job = 0;

int main(int argc, char *argv[])
{
    // array para almacenar la linea
    char line[COMMAND_LINE_SIZE];
    // Inicialización de jobs_list[0] con pid a 0, estado a 'N', y cmd vacío
    jobs_list[0].pid = 0;
    jobs_list[0].estado = 'N';
    /**
     * memset(void *ptr, int value, int num)
     * memset: Rellena un bloque de memoria con un valor específico.
     * @param ptr: Puntero al bloque de memoria que se llenará.
     * @param value: Valor que se usará para llenar cada byte del bloque de memoria.
     * @param num: Número de bytes a establecer en el valor especificado.
     * @return Un puntero al bloque de memoria `ptr` que ha sido rellenado.
     *
     * Uso en el código: Establece todos los bytes de `jobs_list[0].cmd` a `'\0'`,
     * asegurando que el campo `cmd` esté vacío.
     */
    memset(jobs_list[0].cmd, '\0', COMMAND_LINE_SIZE);

    // Guardar el nombre del comando en mi_shell a partir de argv[0]
    strncpy(mi_shell, argv[0], COMMAND_LINE_SIZE);
    mi_shell[COMMAND_LINE_SIZE - 1] = '\0'; // Asegurar terminación de la cadena
    signal(SIGINT, ctrlc);
    signal(SIGCHLD, reaper);
    // bucle infinito
    while (1)
    {
        imprimir_prompt();
        if (read_line(line))
        {
            execute_line(line);
        }
    }
    return 0;
}
void imprimir_prompt()
{
    char *user = getenv("USER"); // Obtener el usuario
    if (user == NULL)
    {
        user = "usuario"; // Asignar valor predeterminado si es NULL
    }

    char cwd[PATH_MAX];

    // Si obtenemos bien el directorio
    if (getcwd(cwd, sizeof(cwd)) != NULL)
    {
        printf(CYAN_T NEGRITA "%s:" RESET, user);      // Imprimir Usuario
        printf(MAGENTA_T NEGRITA "%s " RESET, cwd);    // Imprimir Directorio actual
        printf(MAGENTA_T NEGRITA "%c " RESET, PROMPT); // Imprimir Símbolo PROMPT
    }
    else
    {
        perror("Error");
    }

    fflush(stdout); // Forzar el vaciado del buffer de salida
}
char *read_line(char *line)
{

    // leer linea
    char *ret = fgets(line, COMMAND_LINE_SIZE, stdin);
    // si es diferente de NULL o que empieze por #
    if (ret && strlen(line) > 0)
    {
        /**
         * memchr busca la primera aparación de un caracter en los primeros n bytes de una secuencia
         * void *memchr(const void *ptr, int value, size_t num);
         * ptr: punerto el area de memoria donde bsucar
         * value: caracter que estamos buscando
         * num: max num bytes en ptr donde buscar el caracter value
         * return: puntero a la aparición del caracter, NULL si no lo encuentra
         */
        char *pos = memchr(line, '\n', strlen(line));
        if (pos != NULL)
        {
            *pos = '\0';
        }
    }
    else if (ret == NULL)
    { /*si NULL o EOF*/
        printf("\n");
        if (feof(stdin))
        { /*se ha pulsado CTRL+D*/
            fprintf(stderr, VERDE_T "BYE SIS\n" RESET);
            exit(0);
        }
    }
    else
    {
        ret = NULL;
    }
    return ret;
}

int execute_line(char *line)
{
    char *args[ARGS_SIZE];
    char temp[COMMAND_LINE_SIZE];
    char *eliminador;
    int backg = 0;

    // Guardar la línea original en `temp` antes de analizarla y corregir comentarios
    memset(temp, '\0', sizeof(temp));
    strcpy(temp, line);
    eliminador = strchr(temp, '#');
    if (eliminador != NULL)
    {
        *eliminador = '\0'; // Eliminar comentarios en la línea
    }

    int tok = parse_args(args, line); // Obtener los tokens de la línea de comandos

    // Si hay tokens, comprobar si es un comando interno
    if (tok > 0)
    {
        int ext = check_internal(args);
        if (ext == 0)
        { // Comando externo

            pid_t pid = fork();
            backg = is_background(args);
            int estado;

            if (pid < 0)
            {
                perror("fork");
                return -1;
            }

            if (pid == 0)
            { // PROCESO HIJO
                signal(SIGINT, SIG_IGN);
                signal(SIGTSTP, SIG_IGN);
                is_output_redirection(args);
                if (execvp(args[0], args) == -1)
                {
                    fprintf(stderr, "%s: no se encontró la orden\n", args[0]);
                    exit(-1); // Terminar el hijo si execvp falla
                }
            }
            else
            { // PROCESO PADRE
                if (backg)
                {
                    jobs_list_add(pid, 'E', line);
                    printf("[%d]\t%d\t%c\t%s\n", n_job, pid, 'E', line);
                }

                jobs_list[0].pid = pid;
                jobs_list[0].estado = 'E';
                strncpy(jobs_list[0].cmd, line, COMMAND_LINE_SIZE);

                fprintf(stderr, GRIS_T "[execute_line()→ PID padre: %d (%s)]\n", getpid(), mi_shell);
                fprintf(stderr, GRIS_T "[execute_line()→ PID hijo: %d (%s)]\n", pid, line);

                /**  Bloquear hasta que llegue una señal
                while (jobs_list[0].pid > 0)
                {
                    pause();
                }*/

                // Esperar a que el hijo termine
                waitpid(pid, &estado, 0);
                jobs_list[0].pid = 0;
                jobs_list[0].estado = 'N';
                memset(jobs_list[0].cmd, 0, COMMAND_LINE_SIZE);
            }
        }
        return 1;
    }
    return 0;
}
/**
 * parse_args divide una cadena en una serie de tokens
 * @param args: array de punteros a char
 * @param line: cadena de entrada
 * @return número de tokens
 * @return -1 si hay un error
 */
int parse_args(char **args, char *line)
{
    char *tok;
    int contTok = 0;

    /**
     * strtok divide una cadena en una serie de tokens
     * @param line: cadena de entrada
     * @param delimitador: cadena de caracteres que delimitan los tokens
     * @return puntero al token
     */
    tok = strtok(line, " \t\n\r");

    // bucle de lectura de todos los tokens de la linea
    while (tok != NULL)
    {
        /**
        if (tok == "#")
        {
            // ignorar los comentarios
            args[contTok] = NULL;
            break;
        }*/

        args[contTok] = tok;
        contTok++;
        // Obtenemos el siguiente token
        tok = strtok(NULL, " \t\n\r");
    }
    // final tokens
    args[contTok] = NULL;

    // MOstramos los tokens
    for (int j = 0; j < contTok; j++)
    {
        if ((char)*args[j] == '#')
        {
#if DEBUGN1
            fprintf(stderr, GRIS_T "[parse_args()→ token %d: %s]\n" RESET, j, args[j]);
#endif
            args[j] = NULL;
#if DEBUGN1
            fprintf(stderr, GRIS_T "[parse_args()→ token %d corregido: %s]\n" RESET, j, args[j]);
#endif
            return -1;
        }
        else
        {
#if DEBUGN1
            fprintf(stderr, GRIS_T "[parse_args()→ token %d: %s]\n" RESET, j, args[j]);
#endif
        }
    }
#if DEBUGN1
    fprintf(stderr, GRIS_T "[parse_args()→ token %d: %s]\n" RESET, contTok, args[contTok]);
#endif
    return contTok;
}
/**
 * check_internal comprueba si el comando es interno
 * @param args: array de punteros a char
 * @return 1 si es interno
 * @return 0 si es externo
 */
int check_internal(char **args)
{
    const char *coms[] = {"cd", "export", "source", "jobs", "fg", "bg"};
#if DEBUGN1
    fprintf(stderr, GRIS_T "[check_internal()→ Comando recibido: %s]\n" RESET, args[0]);
#endif

    if (strcmp(args[0], coms[0]) == 0)
    {
        internal_cd(args);
    }
    else if (strcmp(args[0], coms[1]) == 0)
    {
        internal_export(args);
    }
    else if (strcmp(args[0], coms[2]) == 0)
    {
        internal_source(args);
    }
    else if (strcmp(args[0], coms[3]) == 0)
    {
        internal_jobs(args);
    }
    else if (strcmp(args[0], coms[4]) == 0)
    {
        internal_fg(args);
    }
    else if (strcmp(args[0], coms[5]) == 0)
    {
        internal_bg(args);
    }
    else if (strcmp(args[0], "exit") == 0)
    {
        fprintf(stderr, VERDE_T "bye bye\n" RESET);
        exit(0);
    }
    else
    {
        // commands externs
        return 0;
    }
    return 1;
}
/**
 * internal_cd cambia el directorio actual
 * @param args: array de punteros a char
 * @return 1 si tiene éxito
 * @return -1 si hay un error
 */
int internal_cd(char **args)
{
    char buf[COMMAND_LINE_SIZE]; // Buffer para almacenar el directorio actual

    // Caso: comando "cd" sin argumentos (ir al directorio HOME)
    if (args[1] == NULL)
    {
        /**
         * getenv()
         */
        if (chdir(getenv("HOME")) == -1)
        {
            perror(ROJO_T "Error al cambiar al directorio HOME" RESET);
            return -1;
        }
    }
    // Caso: comando "cd" con uno o más argumentos
    else
    {
        char temp[COMMAND_LINE_SIZE];
        char path[COMMAND_LINE_SIZE] = {0};
        strcpy(temp, args[1]);

        // Bucle para concatenar argumentos adicionales si existen
        for (int i = 2; args[i] != NULL; i++)
        {
            strcat(temp, " ");
            strcat(temp, args[i]);
        }

        // Eliminación de caracteres especiales
        char *dir = eliminar_chars(temp);
        // Si hay comillas simples, dobles o caracteres de escape
        if (strchr(args[1], '\'') || strchr(args[1], '"') || strchr(args[1], '\\')) {
            int i = 1; // Inicio en args[1]
            while (args[i] != NULL) {
                strcat(path, args[i]);
                if (args[i + 1] != NULL) {
                    strcat(path, " "); // Añadir espacio entre argumentos
                }
                i++;
            }
        } else { // Directorio simple
            strcpy(path, args[1]);
        }

        // Cambiar al directorio especificado
        if (chdir(dir) == -1)
        {
            perror("chdir");
            return -1;
        }
    }
// Mostrar el directorio actual
    if (getcwd(buf, sizeof(buf)) != NULL) {
#if DEBUGN1
        fprintf(stderr, GRIS_T "[internal_cd()→ PWD: %s]\n" RESET, buf);
#endif
    } else {
        perror(ROJO_T "getcwd() error" RESET);
        return -1;
    }
    return 1;
}

// Función para eliminar caracteres especiales '', "", y /
char *eliminar_chars(char *str)
{
    static char clean_str[256];
    int j = 0;
    for (int i = 0; str[i] != '\0'; i++)
    {
        // Omite caracteres ' " /
        if (str[i] != '\'' && str[i] != '\"' && str[i] != '/')
        {
            clean_str[j++] = str[i];
        }
    }
    clean_str[j] = '\0';
    return clean_str;
}

int internal_export(char **args)
{
    char *nombre = NULL;
    char *valor = NULL;
    // Verificamos el formato
    if (args[1] == NULL || strchr(args[1], '=') == NULL)
    {
        fprintf(stderr, GRIS_T "[internal_export()->nombre: %s]\n" RESET, nombre);
        fprintf(stderr, GRIS_T "[internal_export()->valor: %s]\n" RESET, valor);
        fprintf(stderr, ROJO_T "Error de Sintaxis.Uso: export NOMBRE=VALOR\n");
        return -1;
    }
    // Dividir args[1] en NOMBRE y VALOR usando strtok
    nombre = strtok(args[1], "=");
    valor = strtok(NULL, "");

    // verificamso que ambas partes son válidas
    if (nombre == NULL || valor == NULL)
    {
        fprintf(stderr, GRIS_T "[internal_export()->nombre: %s]\n" RESET, nombre);
        fprintf(stderr, GRIS_T "[internal_export()->valor: %s]\n" RESET, valor);
        fprintf(stderr, ROJO_T "Error de Sintaxis.Uso: export NOMBRE=VALOR\n");
        return -1;
    }
    fprintf(stderr, GRIS_T "[internal_export()->nombre: %s]\n" RESET, nombre);
    fprintf(stderr, GRIS_T "[internal_export()->valor: %s]\n" RESET, valor);
    fprintf(stderr, GRIS_T "[internal_export()->antiguo valor para USER: %s]\n" RESET, getenv("USER"));
    setenv(nombre, valor, 1);
    fprintf(stderr, GRIS_T "[internal_export()->nuevo valor para USER: %s]\n" RESET, getenv("USER"));
    return 1;
}

int internal_source(char **args)
{
    FILE *file;
    char line[COMMAND_LINE_SIZE];

    // Verificación de los argumentos
    if (args[1] == NULL)
    {
        fprintf(stderr, ROJO_T "Error de sintaxis.Uso: source <nobre_fichero>\n" RESET);
        return -1;
    }

    // Abrir el archivo en modo de lectura
    file = fopen(args[1], "r");
    if (file == NULL)
    {
        fprintf(stderr, ROJO_T "Error: %s\n" RESET, strerror(errno));
        return -1;
    }

    // Leer línea a línea del archivo
    while (fgets(line, sizeof(line), file) != NULL)
    {
        // Eliminar el salto de línea al final de la línea leída
        char *newline = strchr(line, '\n');
        if (newline != NULL)
        {
            // poner en la direccion del caracter '\n' el caracter '\0'
            *newline = '\0';
        }

        // Forzar el vaciado del stream del archivo
        fflush(file);

        // Ejecutar la línea leída usando execute_line
        execute_line(line);
    }

    // Cerrar el archivo
    fclose(file);

    return 1;
}
int internal_jobs(char **args)
{
    for (size_t i = 1; i <= n_job; i++)
    {
        printf("[%ld] %d %c %s\n", i, jobs_list[i].pid, jobs_list[i].estado, jobs_list[i].cmd);
    }
    return 1;
}

int internal_fg(char **args)
{
    int pos = atoi(args[1]);
    struct info_job *job = &jobs_list[pos];

    if (pos <= 0 || pos < n_job)
    {
        fprintf(stderr, ROJO_T "fg %d: No existe ese trabajo\n" RESET, pos);
        return 0;
    }

    if (job->estado == 'D')
    {
        kill(job->pid, SIGCONT);
        fprintf(stderr, GRIS_T "[internal_fg()-> Señal %d (SIGCONT) enviada a %d (%s)]\n" RESET, SIGCONT, job->pid, job->cmd);
    }

    // Eliminacion del &
    char *ptr;
    ptr = strchr(job->cmd, '&');
    if (ptr != NULL)
    {
        *ptr = '\0';
    }

    // mover a foreground
    jobs_list[0] = *job;
    jobs_list[0].estado = 'E';
    jobs_list_remove(pos);
    // Muestra por pantalla
    printf("%s\n", jobs_list[0].cmd);
    while (jobs_list[0].pid > 0)
    {
        pause();
    }
    return 1;
}

int internal_bg(char **args)
{
    if (args[1] != NULL)
    {
        int pos = atoi(args[1]);
        struct info_job *job = &jobs_list[pos];
        if (pos <= 0 || pos < n_job)
        {
            fprintf(stderr, ROJO_T "fg %d: No existe ese trabajo\n" RESET, pos);
            return 0;
        }
        else if (job->estado = 'E')
        {
            fprintf(stderr, ROJO_T "bg %d: el trabajo ya esta en 2º plano\n" RESET, pos);
        }
        else
        {
            job->estado = 'E';
            kill(job->pid, SIGCONT);
            strcat(job->cmd, " &");
            printf("[%d]\t%d\t%c\t%s\n", pos, job->pid, job->estado, job->cmd);
        }
        fprintf(stderr, GRIS_T "[internal_bg()→ señal %d (SIGCONT) enviada a %d (%s)]\n" RESET, SIGCONT, job->pid, job->cmd);
        return 1;
    }
    return 0;
}
void ctrlc(int signum)
{
    printf("\n[ctrlc()→ Señal SIGINT recibida]\n");
    struct info_job job = jobs_list[0];
    if (job.pid > 0)
    {
        if (strcmp(job.cmd, mi_shell) != 0)
        {
            printf("[ctrlc()→ Enviando SIGTERM al proceso hijo %d (%s)]\n", jobs_list[0].pid, jobs_list[0].cmd);
            kill(job.pid, SIGTERM);
        }
        else
        {
            printf("[ctrlc()→ No hay proceso en foreground o es el shell]\n");
        }
    }
    else
    {
        printf("señal SIGTERM no enviada: no hay ningun proceso en foreground");
    }
}
void reaper(int signum)
{
    signal(SIGCHLD, reaper);
    int status = 0;
    int pid = 0;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
    {
        printf("[reaper()→ Proceso hijo %d (%s) finalizado ", pid, jobs_list[0].cmd);
        if (WIFEXITED(status))
        {
            printf("con exit code %d]\n", WEXITSTATUS(status));
        }
        else if (WIFSIGNALED(status))
        {
            printf("por señal %d]\n", WTERMSIG(status));
        }

        jobs_list[0].pid = 0;
        jobs_list[0].estado = 'F';
        memset(jobs_list[0].cmd, '\0', COMMAND_LINE_SIZE);
    }
}
int is_background(char **args)
{
    for (int i = 0; args[0] != NULL; i++)
    {
        if (strcmp(args[i], "&") == 0)
        {
            args[0] = NULL;
            return 1;
        }
    }
    return 0;
}
int jobs_list_add(pid_t pid, char estado, char *cmd)
{
    n_job++;
    if (n_job < N_JOBS)
    {
        jobs_list[n_job].pid = pid;
        jobs_list[n_job].estado = estado;
        strcpy(jobs_list[n_job].cmd, cmd);
        return 1;
    }
    return 0;
}
int job_list_find(pid_t pid)
{
    for (size_t i = 0; i < sizeof(jobs_list); i++)
    {
        if (jobs_list[i].pid == pid)
        {
            return i;
        }
    }
    return -1;
}
int jobs_list_remove(int pos)
{
    jobs_list[pos].pid = jobs_list[n_job].pid;
    jobs_list[pos].estado = jobs_list[n_job].estado;
    strcmp(jobs_list[pos].cmd, jobs_list[n_job].cmd);
    n_job--;
    return 1;
}
void ctrlz(int signum)
{
    pid_t pid;
    pid = getpid();
    if (jobs_list[0].pid)
    {
        if (strcmp(jobs_list[0].cmd, mi_shell) != 0)
        {
            kill(jobs_list[0].pid, SIGSTOP);
            fprintf(stderr, GRIS_T "\n[ctrlz()->Soy el proceso con PID %d (%s), el proceso en foreground es %d (%s)]" RESET, pid, mi_shell, jobs_list[0].pid, jobs_list[0].cmd);
            fprintf(stderr, GRIS_T "\n[ctrlz()->Señal %d enviada a %d (%s) por %d (%s)]\n" RESET, SIGTERM, jobs_list[0].pid, jobs_list[0].cmd, pid, mi_shell);
            jobs_list[0].estado = 'D';
            jobs_list_add(jobs_list[0].pid, jobs_list[0].estado, jobs_list[0].cmd);

            // Reset
            jobs_list[0].pid = 0;
            jobs_list[0].estado = 'F';
            memset(jobs_list[0].cmd, '\0', COMMAND_LINE_SIZE);
        }
        else
        {
            fprintf(stderr, GRIS_T "señal SIGSTOP no enviada por %d (%s): el proceso en foreground es el shell", getpid(), mi_shell);
        }
    }
    else
    {
        fprintf(stderr, GRIS_T "señal SIGSTOP no enviada por %d (%s): no hay ningun proceso en foreground", getpid(), mi_shell);
    }
}
int is_output_redirection(char **args)
{
    int f;
    int i = 0;
    while (args[i] != NULL)
    {
        if (*args[i] == '>')
        {
            if (args[i + 1] != NULL)
            {
                f = open(args[i + 1], O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
                if (f != -1)
                {
                    args[i] = NULL;
                    args[i + 1] = NULL;
                    if (dup2(f, 1) == -1)
                    {
                        perror("dup2");
                    }
                    if (close(f) == -1)
                    {
                        perror("close");
                    }
                    return 1;
                }
                else
                {
                    fprintf(stderr, ROJO_T "Error al crear el fichero" RESET);
                    return 0;
                }
            }
            else
            {
                fprintf(stderr, ROJO_T "Sintaxis incorecta" RESET);
                return 0;
            }
        }
        i++;
    }
    return 0;
}
