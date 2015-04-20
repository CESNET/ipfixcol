/**
 * \file input_test.c
 * \author Petr Velan <petr.velan@cesnet.cz>
 * \brief IPFIX Colector input plugin tester

 * Copyright (C) 2015 CESNET, z.s.p.o.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of the Company nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * ALTERNATIVELY, provided that this notice is retained in full, this
 * product may be distributed under the terms of the GNU General Public
 * License (GPL) version 2 or later, in which case the provisions
 * of the GPL apply INSTEAD OF those given above.
 *
 * This software is provided ``as is, and any express or implied
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose are disclaimed.
 * In no event shall the company or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even
 * if advised of the possibility of such damage.
 *
 */

/**
 * \defgroup tests ipfix plugin tests
 *
 * This group contains test utilities for input and storage plugins.
 *
 */

/**
 * \defgroup inputPluginTest ipfixcol input plugin test
 * \ingroup tests
 *
 * This is a standalone test utility for debugging input plugins
 *
 * @{
 */

#include <stdio.h>
#include <dlfcn.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <arpa/inet.h>

#include <ipfixcol.h>

/* timeout for tested functions */
#define TIMEOUT 30

/* accepted program arguments */
#define ARGUMENTS "f:t:p:u:s:h64"

/* all functions in the plugin have int return type */
typedef int (*func_type)();

/* configuration passed to each plugin call */
void *config;

/* protocol family, default is IPv4 */
int af = AF_INET;

/**
 * \brief Test send data function
 *
 * Function sends data to localhost on specified UDP or TCP port
 *
 * \param[in] port String specifying connection port
 * \param[in] socktype Type of the socket (SOCK_STREAM for TCP or SOCK_DGRAM for UDP)
 * \param[out] error_msg String with error message if something fails
 * \return 0 on success, 1 on socket error or 2 when plugin is not listening
 */
int send_data(char *port, int socktype, char **error_msg)
{
    int sock;
    struct addrinfo hints, *addrinfo;
    
    memset(&hints, 0, sizeof hints);
    hints.ai_family = af;
    hints.ai_socktype = socktype;

    /* get server address */
    if (getaddrinfo("localhost", port, &hints, &addrinfo) != 0) {
        *error_msg = "Cannot get server info";
        return 1;
    }
    
    /* create socket */
    sock = socket(addrinfo->ai_family, addrinfo->ai_socktype, addrinfo->ai_protocol);
    if (sock == -1) {
        *error_msg = "Cannot create new socket";
        return 1;
    }

    /* connect to server */
    if (connect(sock, addrinfo->ai_addr, addrinfo->ai_addrlen) == -1) {
        close(sock);
        *error_msg = "Cannot connect to plugin";
        return 2;
    }

    /* send data to plugin */
    struct ipfix_header msg;
    msg.length = sizeof(struct ipfix_header);
    msg.observation_domain_id = 1;
    if (sendto(sock, (void *) &msg, sizeof(struct ipfix_header), 0, addrinfo->ai_addr, addrinfo->ai_addrlen) == -1)
    {
        *error_msg = "Cannot send data to plugin";
        return 1;
    }

    freeaddrinfo(addrinfo);
    close(sock);

    return 0;
}


/**
 * \brief Function for testing plugin's input_init function
 *
 * \param[in] function The function that should be tested
 * \param[in] params Parameters passed to plugin's input_init functioni
 * \return 0 on succes, 1 on failure
 */
int test_input_init(func_type function, char *params)
{ 
    /* try to call the function */
    int ret = function(params, &config);

    if (config == NULL) printf("  INFO: plugin did not set any configuration\n");

    printf("  input_init function returned %d ... ", ret);
    printf("%s\n", (ret==0)? "OK":"ERROR");

    return ret>0?1:0;
}

/**
 * \brief Function for testing plugin's get_packet function
 *
 * When UDP or TCP port is specified, sends test packet to plugin on this port.
 * Plugin must be set to listen, otherwise the test wil fail.
 *
 * \param[in] function The function that should be tested
 * \param[in] udp_port String containing the UDP port to send data
 * \param[in] tcp_port String containing the TCP port to send data
 * \return 0 on success, 1 on failure
 */
int test_get_packet(func_type function, char *udp_port, char *tcp_port)
{
    struct input_info_network *input_info = NULL;
    char *packet = NULL, *error_msg = NULL;
    int error = 0, ret, data_sent = 0;
    char src_addr[INET6_ADDRSTRLEN];

    /* send UDP data to the plugin */
    if (udp_port != NULL) 
    {
        printf("  Sending UDP data... ");
        ret = send_data(udp_port, SOCK_DGRAM, &error_msg);
        if (ret == 0) data_sent = 1;

        if (data_sent != 0) printf("OK\n");
        else printf("FAILED (%s)\n", error_msg);
    }

    /* send TCP data to the plugin */
    if (tcp_port != NULL)
    {
        printf("  Sending TCP data... ");
        ret = send_data(tcp_port, SOCK_STREAM, &error_msg);
        if (ret == 0) data_sent = 1;
        if (ret > 1) error++;
        
        if (data_sent != 0) printf("OK\n");
        else printf("FAILED (%s)\n", error_msg);
    }
    
    /* try to call the function */
    ret = function(config, &input_info, &packet);

    if (ret <= 0) error++;

    if (packet == NULL) 
    {
        if (data_sent == 1) error++; /* data sent but not received => error */
        printf("  %s: plugin did not return any packet data\n", (data_sent!=0)? "ERROR": "INFO");
    } else if (ret > 0) 
    {
        printf("  Expecting some data from plugin... Got %d bytes... OK\n", ret);
    } else 
    {
        printf("  Error: Expected some data from plugin, got return code %d\n", ret);
    }

    if (input_info == NULL) 
    {
        if (data_sent == 1) error++;    
        printf("  %s: plugin did not return any input_info\n", (data_sent!=0)? "ERROR": "INFO");
    } else 
    {
        if (input_info->l3_proto == 4) 
        {
             inet_ntop(AF_INET, &input_info->src_addr.ipv4.s_addr, src_addr, INET6_ADDRSTRLEN);
        } else
        {
            inet_ntop(AF_INET6, &input_info->src_addr.ipv6.s6_addr, src_addr, INET6_ADDRSTRLEN);
        }

        printf("  INFO: plugin returned input_info (src address: %s, src_port %u)\n", src_addr, input_info->src_port);
    }

    printf("  get_packet function returned %d ... ", ret);
    printf("%s\n", (ret>0)? "OK":"ERROR");

    /* test whether function returns INPUT_CLOSE, TCP only */
    if (data_sent && tcp_port != NULL) {
        ret = function(config, &input_info, &packet);
        if (ret == INPUT_CLOSED) {
            printf("  INFO: second call to get_packet function correctly reported closed connection\n");
        } else {
            printf("  ERROR: second call to get_packet function returned %d, INPUT_CLOSED(%d) expected\n", ret, INPUT_CLOSED);
        }
    }
    
    return error;
}

/**
 * \brief Function for testing plugin's input_close function
 *
 * \param[in] function The function that should be tested
 * \return 0 on success, 1 on failure
 */
int test_input_close(func_type function)
{
    /* try to call the function */
    int ret = function(&config);

    printf("  input_close function returned %d ... ", ret);
    printf("%s\n", (ret==0)? "OK":"ERROR");
    
    return ret>0?1:0;
}

/**
 * \brief Prints how to use the program
 *
 * \param[in] name String with program name
 */
void usage(char *name)
{
    printf("Usage:\n");
    printf("  %s [-s num] -f input_plugin\n\n", name);
    printf("Options:\n");
    printf("  -f input_plugin  specify input plugin to test\n");
    printf("  -s num           set timeout to num seconds for plugin functions. Default is %ds\n", TIMEOUT);
    printf("  -p plugin_config file with xml plugin configuration passed to the plugin input_init function\n");
    printf("  -u udp_port      send test data to UDP port udp_port [4739]. Cannot be used with -t\n");
    printf("  -t tcp_port      send test data to TCP port tcp_port [4739]. Cannot be used with -u\n");
    printf("  -6               use IPv6 to send test data\n");
    printf("  -4               use IPv4 to send test data (default)\n");
    printf("  -h               print usage info\n");
    printf("\nWithout -f option print this help\n\n");
}

/**
 * \brief Main function
 *
 * Parses input values, forks and test plugin's functions in the child.
 * Prints test status.
 *
 * \param[in] argc Number of command line arguments
 * \param[out] argv Arrat of Strings with command line arguments
 * \return 0 on success, nonzero on failure
 */
int main(int argc, char *argv[])
{
    unsigned int i;
    /* variables for fork and waitpid */
    int pid, status, options;

    /* number of encountered errors */
    int errors = 0;

    /* give plugin timeout seconds to respond */
    int timeout = TIMEOUT;

    /* the order is important as switch is used to differetiate the cases */
    char *functions[] = {"input_init", "get_packet", "input_close"};
    
    /* function in the plugin*/
    func_type function;

    /* do not wait for plugin function to end */
    options = WNOHANG;

    int c;
    char *input_plugin = NULL;
    char *pc_file = NULL;
    char *udp_port = NULL;
    char *tcp_port = NULL;
    /* parse given parameters */
    while ((c = getopt(argc, argv, ARGUMENTS)) != -1) {
        switch (c) {
        case 'f': /* input plugin file */
            input_plugin = optarg;
            break;

        case 'h': /* show help */
            usage(argv[0]);
            exit(0);

        case 's': /* set timeout */
            timeout = atoi(optarg);
            break;

        case 'p': /* input plugin parameters */
            pc_file = optarg;
            break;

        case 'u':
            udp_port = optarg;
            break;

        case 't':
            tcp_port = optarg;
            break;

        case '6':
            af = AF_INET6;
            break;

        case '4':
            af = AF_INET;
            break;

        default: /* unknown option - print usage */
            usage(argv[0]);
            exit(1);
        }
    }
    
    /* check input parameters */
    if (input_plugin == NULL || (udp_port !=NULL && (atoi(udp_port) > 65535 || atoi(udp_port) < 0)) ||
        (tcp_port != NULL && (atoi(tcp_port) > 65535 || atoi(tcp_port) < 0)) || (udp_port != NULL && tcp_port != NULL))
    {
        usage(argv[0]); 
        exit(1);
    }
    
    /* try to load the plugin */
    void *handle = dlopen(input_plugin, RTLD_LAZY);
    if (!handle) 
    {
        fprintf(stderr, "An error occured while opening the plugin: %s\n", dlerror());
        exit(1);
    }

    /* load plugin config file */
    char *params;
    long pc_size;
    int res;
    FILE *pc = fopen(pc_file, "r");
    if (pc == NULL) 
    {
        fprintf(stderr, "Cannot open plugin configuration file: %s\n", pc_file);    
        exit(1);
    }
    /* obtain file size */
    fseek(pc , 0 , SEEK_END);
    pc_size = ftell(pc);
    rewind(pc);

    /* allocate memory to contain the whole file + terminating zero */
    params = (char*) malloc (sizeof(char)*pc_size + 1);
    if (params == NULL) 
    {
        fprintf(stderr, "Cannot allocate memory for plugin configuration file\n"); 
        exit (1);
    }

    /* copy the file into the buffer */
    res = fread(params,1,pc_size,pc);
    if (res != pc_size) 
    {
        fprintf(stderr, "Cannot read the configuration file\n"); 
        exit(1);
    }
    params[pc_size] = '\0';



    /* fork the process so that buggy plugin does not kill us */
    pid = fork();
    if (pid == 0) /* child process */
    {

        /* go through all the functions*/
        for (i=0; i<sizeof(functions)/sizeof(char*); i++)
        {
            /* get the function from plugin */
            function = dlsym(handle, functions[i]);
            if (!function)
            {
                fprintf(stderr, "An error occured while getting %s function: %s\n", functions[i], dlerror());
                errors++;
                continue;
            }

            /* run the test function and save it's result */
            printf("\nStarting %s function test:\n", functions[i]);

            int function_errors = 0;
            switch (i)
            {
                case 0: function_errors += test_input_init(function, params); break;
                case 1: function_errors += test_get_packet(function, udp_port, tcp_port); break;
                case 2: function_errors += test_input_close(function); break;
                default: fprintf(stderr, "Test cannot handle function %s\n", functions[i]); break;
            }
            errors += function_errors;
            printf("%s test result: %s\n", functions[i], (errors==0)? "SUCCESS": "FAILED");
        }
        return errors;
        
    } else if (pid > 0) /* parent process */
    {
        /* check the child process runing the test */

        /* allow only to run for specified time */
        long tmp_timeout = timeout*1000000;
        int wait_ret = 0;

        while (tmp_timeout > 0 && wait_ret == 0)
        {
            wait_ret = waitpid(pid, &status, options);
            tmp_timeout -= 1000;
            usleep(1000);
        }
        
        /* check whether the plugin function call completed */
        if (wait_ret == 0) /* not completed */
        {
            errors++;
            fprintf(stderr, "Plugin ran longer than %d seconds\n", timeout);
            fprintf(stderr, "Trying to kill the plugin...");
            if (kill(pid, SIGTERM) < 0)
            {
                perror("Plugin cannot be killed: ");
            }
            fprintf(stderr, " plugin killed\n");
        } else { /* completed, check returned status */
            if (!WIFEXITED(status)) 
            {
                fprintf(stderr, "ERROR: Abnormal exit while executing plugin function\n");
                errors++;
            }
            if (WIFSIGNALED(status)) {
                fprintf(stderr, "ERROR: Uncaught signal in plugin function\n");
                errors++;
            }
            if (errors > 0) /* abnormal exit */
            {
                printf("\nTest FAILED, plugin function exited abnormally\n");
            }
            if (errors == 0) /* normal exit, no errors so far */
            {
                errors += WEXITSTATUS(status);
            }
        }
    }

    free(params);

    /* give final report */
    if (!errors) printf("\nAll functions are present and working\n");
    else printf("\nThere are %d errors in the plugin\n", errors);

    return 0;    
}
/**@}*/
