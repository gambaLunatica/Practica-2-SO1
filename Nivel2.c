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
char *eliminar_chars(char *str);

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
        imprimir_prompt();
        if (read_line(line))
        {
            execute_line(line);
        }
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

    // leer linea
    char *ret = fgets(line, COMMAND_LINE_SIZE, stdin);
    // si es diferente de NULL o que empieze por #
    if (ret && *ret != '#')
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
    int tok = parse_args(args, line);
    if (tok > 0)
    {
        check_internal(args);
    }
    return 1;
}

int parse_args(char **args, char *line)
{
    char *tok;
    int contTok = 0;

    // obtenemso el primer token
    /**
     *
     */
    tok = strtok(line, " \t\n\r");

    // bucle de lectura de todos los tokens de la linea
    while (tok != NULL)
    {
        if (tok == NULL)
        {
            args[contTok] = NULL;
            return -1;
            break;
        }

        args[contTok] = tok;
        contTok++;
        // Obtenemos el siguiente token
        tok = strtok(NULL, " \t\n\r");
    }
    //final tokens
    args[contTok] = NULL;
    //MOstramos los tokens
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

int check_internal(char **args){
    const char *coms[] = {"cd","export", "source", "jobs", "fg", "bg"};

    if(strcmp(args[0],coms[0])==0){
        internal_cd(args);
    }
    else if(strcmp(args[0],coms[1])==0){
        internal_export(args);
    }
    else if(strcmp(args[0],coms[2])==0){
        internal_source(args);
    }
    else if(strcmp(args[0],coms[3])==0){
        internal_jobs(args);
    }
    else if(strcmp(args[0],coms[4])==0){
        internal_fg(args);
    }
    else if(strcmp(args[0],coms[5])==0){
        internal_bg(args);
    }
    else if(strcmp(args[0],"exit")==0){
        printf("bye bye\n");
        exit(0);
    }
    else {
        //commands externs
        return 0;
    }
    return 1;
}

int internal_cd(char **args) {
    char buf[256];  // Buffer para almacenar el directorio actual

    // Caso: comando "cd" sin argumentos (ir al directorio HOME)
    if (args[1] == NULL) {
        /**
         * getenv()
         */
        if (chdir(getenv("HOME")) == -1) {
            perror("chdir");
            return -1;
        }
    }
    // Caso: comando "cd" con uno o más argumentos
    else {
        char temp[256];
        strcpy(temp, args[1]);

        // Bucle para concatenar argumentos adicionales si existen
        for (int i = 2; args[i] != NULL; i++) {
            strcat(temp, " ");
            strcat(temp, args[i]);
        }

        // Eliminación de caracteres especiales
        char *dir = eliminar_chars(temp);

        // Cambiar al directorio especificado
        if (chdir(dir) == -1) {
            perror("chdir");
            return -1;
        } else {
            getcwd(buf, sizeof(buf));
            printf("Directorio actual: %s\n", buf);
        }
    }

    return 1;
}

// Función para eliminar caracteres especiales '', "", y /
char *eliminar_chars(char *str) {
    static char clean_str[256];
    int j = 0;
    for (int i = 0; str[i] != '\0'; i++) {
        // Omite caracteres ' " /
        if (str[i] != '\'' && str[i] != '\"' && str[i] != '/') {
            clean_str[j++] = str[i];
        }
    }
    clean_str[j] = '\0';
    return clean_str;
}

int internal_export(char **args) {
    char *nombre = NULL;
    char *valor = NULL;
    //Verificamos el formato
    if(args[1] == NULL || strchr(args[1], '=') == NULL){
        fprintf(stderr,GRIS_T"[internal_export()->nombre: %s]\n"RESET,nombre);
        fprintf(stderr,GRIS_T"[internal_export()->valor: %s]\n"RESET,valor);
        fprintf(stderr,ROJO_T"Error de Sintaxis.Uso: export NOMBRE=VALOR\n");
        return -1;
    }
    // Dividir args[1] en NOMBRE y VALOR usando strtok
    nombre = strtok(args[1], "=");
    valor = strtok(NULL, "");

    //verificamso que ambas partes son válidas
    if(nombre == NULL || valor == NULL){
        fprintf(stderr,GRIS_T"[internal_export()->nombre: %s]\n"RESET,nombre);
        fprintf(stderr,GRIS_T"[internal_export()->valor: %s]\n"RESET,valor);
        fprintf(stderr,ROJO_T"Error de Sintaxis.Uso: export NOMBRE=VALOR\n");
        return -1;
    }
    fprintf(stderr,GRIS_T"[internal_export()->nombre: %s]\n"RESET,nombre);
    fprintf(stderr,GRIS_T"[internal_export()->valor: %s]\n"RESET,valor);
    fprintf(stderr,GRIS_T"[internal_export()->antiguo valor para USER: %s]\n"RESET,getenv("USER"));
    setenv(nombre,valor,1);
    fprintf(stderr,GRIS_T"[internal_export()->nuevo valor para USER: %s]\n"RESET,getenv("USER"));
   return 1;
}

int internal_source(char **args) {
      fprintf(stderr, GRIS_T "[internal_export()→ Esta función ejecutará un fichero de líneas de comandos]\n" RESET);
   return 1;
}

int internal_jobs(char **args) {
      fprintf(stderr, GRIS_T "[internal_export()→ Esta función mostrará el PID de los procesos que no estén en foreground]\n" RESET);
   return 1;
}

int internal_fg(char **args) {
      fprintf(stderr, GRIS_T "[internal_export()→ Envía un trabajo del background al foreground, o reactiva la ejecución en foreground de un trabajo que había sido detenido.]\n" RESET);
   return 1;
}

int internal_bg(char **args) {
      fprintf(stderr, GRIS_T "[internal_export()→ Envía un trabajo del background al foreground, o reactiva la ejecución en foreground de un trabajo que había sido detenido.]\n" RESET);
   return 1;
}