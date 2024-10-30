#define _POSIX_C_SOURCE 200112L
// librerias a usar
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>
#include <linux/limits.h>
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

// Variable para leer una line
#define COMMAND_LINE_SIZE 1024
#define ARGS_SIZE 64
int main()
{
    // array para almacenar la linea
    char line[COMMAND_LINE_SIZE];
    // bucle infinito
    while (1)
    {
        read_line(line);
    }
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
    imprimir_prompt();

    // leer linea
    char *ret = fgets(line, COMMAND_LINE_SIZE, stdin);
    // si es diferente de NULL o que empieze por #
    if (ret && *ret != '#')
    {
        char *pos = memchr(line, '\n', strlen(line));
        *pos = '\0';
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
int parse_args(char **args, char *line)
{
    char *tok;
    int contTok = 0;

    tok = strtok(line, " \t\n");
    if (tok = NULL)
    {
        args[contTok] = NULL;
        return -1;
    }
    args[contTok] = tok;
    contTok++;
    // bucle de lectura de todos los tokens de la linea
    while (tok != NULL)
    {
        args[contTok] = tok;
        contTok++;
        // Obtenemos el siguiente token
        tok = strtok(NULL, " \t\n");
    }
    args[contTok] = NULL;
    for (int j = 0; j < contTok; j++)
    {
        if ((char)*args[j] == '#')
        {
            fprintf(stderr, GRIS_T "[parse_args()→ token %d: %s]\n" RESET, j, args[j]);
            args[j] = NULL;
            fprintf(stderr, GRIS_T "[parse_args()→ token %d corregido: %s]\n" RESET, j, args[j]);
            return -1;
        }
        else
        {
            fprintf(stderr, GRIS_T "[parse_args()→ token %d: %s]\n" RESET, j, args[j]);
        }
    }

    fprintf(stderr, GRIS_T "[parse_args()→ token %d: %s]\n" RESET, contTok, args[contTok]);
    return contTok;
}