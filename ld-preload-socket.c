#define _GNU_SOURCE

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <malloc.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>

#define UNIX_SOCKET_PATHS_ENV_VAR "LD_PRELOAD_SOCKET_UNIX_SOCK_MAP"
#define INET_SOCKET_PORTS_ENV_VAR "LD_PRELOAD_SOCKET_INET_PORT_MAP"

#define SEPARATORS ":,\n"

// internals
#define LOG_PREFIX "LD_PRELOAD_SOCKET"
#define SOURCE 1
#define TARGET 2

typedef int (*orig_bind_func_type)(int fd, const struct sockaddr *addr, socklen_t len);
typedef int (*orig_connect_func_type)(int fd, const struct sockaddr *addr, socklen_t len);
typedef int (*orig_unlink_func_type)(const char *pathname);

typedef struct
{
    int source;
    int target;
} inet_socket_port_mapping;

typedef struct
{
    char *source;
    char *target;
} unix_socket_path_mapping;

__thread inet_socket_port_mapping *inet_socket_ports = NULL;
__thread int inet_socket_ports_len = 0;

__thread unix_socket_path_mapping *unix_socket_paths = NULL;
__thread int unix_socket_paths_len = 0;

static void trim(char *str)
{
    int i;
    int begin = 0;
    int end = strlen(str) - 1;
    while (isspace((unsigned char)str[begin]))
    {
        begin++;
    }

    while (end >= begin && isspace((unsigned char)str[end]))
    {
        end--;
    }

    // Shift all characters back to the start of the string array
    for (i = begin; i <= end; i++)
    {
        str[i - begin] = str[i];
    }

    str[i - begin] = '\0';
}

static void parse_unix_socket_paths()
{
    char *env_variable = getenv(UNIX_SOCKET_PATHS_ENV_VAR);

    char *content = malloc(strlen(env_variable) + 1);
    strcpy(content, env_variable); // strtok needs a copy because if modifies the string

    if (unix_socket_paths != NULL)
    {
        for (size_t i = 0; i < unix_socket_paths_len; i++)
        {
            free(unix_socket_paths[i].source);
            free(unix_socket_paths[i].target);
        }
        free(unix_socket_paths);
        unix_socket_paths = NULL;
        unix_socket_paths_len = 0;
    }

    int next = SOURCE;
    char *source_path = NULL;
    char *target_path = NULL;

    char *token = strtok(content, SEPARATORS);
    while (token != NULL)
    {
        switch (next)
        {
        case SOURCE:
            unix_socket_paths_len++;

            unix_socket_paths = realloc(unix_socket_paths, sizeof(*unix_socket_paths) * unix_socket_paths_len);
            source_path = malloc(sizeof(char) * (strlen(token) + 1));
            strcpy(source_path, token);
            trim(source_path);

            unix_socket_paths[unix_socket_paths_len - 1].source = source_path;
            unix_socket_paths[unix_socket_paths_len - 1].target = source_path;

            next = TARGET;
            break;

        case TARGET:
            target_path = malloc(sizeof(char) * (strlen(token) + 1));
            strcpy(target_path, token);
            trim(target_path);

            unix_socket_paths[unix_socket_paths_len - 1].target = target_path;

            next = SOURCE;
            break;
        }

        token = strtok(NULL, SEPARATORS);
    }

    free(content);
}

static const char *map_unix_socket_path(const char *source_path)
{
    if (unix_socket_paths == NULL) // parse env variable only once
    {
        parse_unix_socket_paths();
    }

    for (int i = 0; i < unix_socket_paths_len; i++)
    {
        if (strlen(source_path) > 0 && strncmp(unix_socket_paths[i].source, source_path, strlen(source_path)) == 0)
        {
            return unix_socket_paths[i].target;
        }
    }

    return NULL; // no mapping found
}

static void update_unix_socket_path(struct sockaddr_un *addr, const char *func_name)
{
    const char *mapped_path = map_unix_socket_path(addr->sun_path);
    if (mapped_path == NULL) // No mapping defined
    {
        return;
    }

    fprintf(stderr, "%s: Mapping AF_UNIX %s(%s) to %s(%s)\n", LOG_PREFIX, func_name, addr->sun_path, func_name, mapped_path);
    strncpy(addr->sun_path, mapped_path, sizeof(addr->sun_path));
    addr->sun_path[sizeof(addr->sun_path) - 1] = '\0';
}

static void parse_inet_socket_ports()
{
    char *env_variable = getenv(INET_SOCKET_PORTS_ENV_VAR);
    if (env_variable == NULL)
    {
        return;
    }

    char *content = malloc(sizeof(char) * (strlen(env_variable) + 1));
    strcpy(content, env_variable); // strtok needs a copy because it modifies the string

    if (inet_socket_ports != NULL)
    {
        free(inet_socket_ports);
        inet_socket_ports = NULL;
        inet_socket_ports_len = 0;
    }

    int next = SOURCE;
    int source_port = 0;
    int target_port = 0;

    char *token = strtok(content, SEPARATORS);
    while (token != NULL)
    {
        switch (next)
        {
        case SOURCE:
            inet_socket_ports_len++;
            inet_socket_ports = realloc(inet_socket_ports, sizeof(*inet_socket_ports) * inet_socket_ports_len);

            source_port = atoi(token);
            inet_socket_ports[inet_socket_ports_len - 1].source = source_port;
            inet_socket_ports[inet_socket_ports_len - 1].target = 0;

            next = TARGET;
            break;
        case TARGET:
            target_port = atoi(token);
            inet_socket_ports[inet_socket_ports_len - 1].target = target_port;

            next = SOURCE;
            break;
        }

        token = strtok(NULL, SEPARATORS);
    }

    free(content);
}

static int map_inet_socket_port(int source_port)
{
    if (inet_socket_ports == NULL) // parse env variable only once
    {
        parse_inet_socket_ports();
    }

    for (size_t i = 0; i < inet_socket_ports_len; i++)
    {
        if (inet_socket_ports[i].source == source_port)
        {
            return inet_socket_ports[i].target;
        }
    }

    return source_port; // no mapping found
}

static void update_inet_socket_port(struct sockaddr_in *addr, const char *func_name)
{
    int source_port = ntohs(addr->sin_port); // addr->sin_port is in network byte order

    int target_port = map_inet_socket_port(source_port);
    if (target_port == source_port) // No mapping defined
    {
        return;
    }

    fprintf(stderr, "%s: Mapping AF_INET %s(%d) to %s(%d)\n", LOG_PREFIX, func_name, source_port, func_name, target_port);
    addr->sin_port = htons(target_port);
}

void update_socket(const struct sockaddr *addr, const char *func_name)
{
    switch (addr->sa_family)
    {
    case AF_UNIX:
        update_unix_socket_path((struct sockaddr_un *)addr, func_name);
        break;
    case AF_INET:
        update_inet_socket_port((struct sockaddr_in *)addr, func_name);
        break;
    default:
        break;
    }
}

// Intercept bind() for socket servers.
int bind(int fd, const struct sockaddr *addr, socklen_t len)
{
    update_socket(addr, "bind");

    orig_bind_func_type orig_func;
    orig_func = (orig_bind_func_type)dlsym(RTLD_NEXT, "bind");

    return orig_func(fd, addr, len);
}

// Intercept connect() for socket clients.
int connect(int fd, const struct sockaddr *addr, socklen_t len)
{
    update_socket(addr, "connect");

    orig_connect_func_type orig_func;
    orig_func = (orig_connect_func_type)dlsym(RTLD_NEXT, "connect");

    return orig_func(fd, addr, len);
}

// Socket servers usually use the unlink-then-bind pattern when opening AF_UNIX sockets.
// Intercept unlink() to make that work correctly.
int unlink(const char *pathname)
{
    const char *path_to_unlink = pathname;

    const char *mapped_pathname = map_unix_socket_path(pathname);
    if (mapped_pathname != NULL)
    {
        fprintf(stderr, "%s: Mapping AF_UNIX unlink(%s) to unlink(%s)\n", LOG_PREFIX, pathname, mapped_pathname);
        path_to_unlink = mapped_pathname;
    }

    orig_unlink_func_type orig_func;
    orig_func = (orig_unlink_func_type)dlsym(RTLD_NEXT, "unlink");

    return orig_func(path_to_unlink);
}

// Print all mappings once at startup
__attribute__((constructor)) static void setup()
{
    if (unix_socket_paths == NULL)
    {
        parse_unix_socket_paths();
    }
    fprintf(stderr, "%s: %d mappings defined for UNIX socket paths\n", LOG_PREFIX, unix_socket_paths_len);
    for (size_t i = 0; i < unix_socket_paths_len; i++)
    {
        fprintf(stderr, "%s:    '%s' -> '%s'\n", LOG_PREFIX, unix_socket_paths[i].source, unix_socket_paths[i].target);
    }

    if (inet_socket_ports == NULL)
    {
        parse_inet_socket_ports();
    }
    fprintf(stderr, "%s: %d mappings defined for INET socket ports\n", LOG_PREFIX, inet_socket_ports_len);
    for (size_t i = 0; i < inet_socket_ports_len; i++)
    {
        fprintf(stderr, "%s:    %d -> %d\n", LOG_PREFIX, inet_socket_ports[i].source, inet_socket_ports[i].target);
    }
}
