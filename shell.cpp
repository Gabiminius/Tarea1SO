#include <iostream>
#include <string>
#include <sstream>
#include <unistd.h>
#include <pwd.h>
#include <sys/wait.h>
#include <vector>
#include <array>
#include <sys/resource.h>
#include <fstream>
#include <signal.h> 
#include <time.h>   

#define RESET   "\033[0m"
#define GREEN   "\033[32m"  // color prompt
#define RED     "\033[31m"  // color errores

using namespace std;

//utilidad miprof
static pid_t g_child = -1;

void kill_child(int) {
    if(g_child > 0){
        kill(g_child, SIGKILL);
    }
}

// Función para separar un comando por pipes 
vector<vector<string>> separadoPorPipes(const vector<string>& parse_command) {
    vector<vector<string>> result;
    vector<string> current;

    for (const auto& word : parse_command) {
        if (word == "|") {
            if (!current.empty()) {
                result.push_back(current);
                current.clear();
            }
        } else {
            current.push_back(word);
        }
    }
    if (!current.empty()) result.push_back(current);

    return result;
}

int main() {
    while (true) {
        // prompt 
        struct passwd* pw = getpwuid(getuid());
        string username = pw->pw_name;
        char hostname[1024];
        gethostname(hostname, sizeof(hostname));

        char cwd[1024];
        getcwd(cwd, sizeof(cwd));   // obtiene directorio actual

        cout << GREEN << username << "@" << hostname << ":" << cwd << "$ " << RESET;
        string command, word;
        getline(cin, command);

        // parsear el comando 
        istringstream iss(command);
        vector<string> parse_command;
        while (iss >> word) parse_command.push_back(word);

        if (parse_command.empty()) continue;

        //comandos internos
        if (parse_command[0] == "exit") {
            cout << "Saliendo de la MirkoShell ᓚᘏᗢ..." << endl;
            exit(0);
        }
        else if (parse_command[0] == "cd") {
            if (parse_command.size() < 2) {
                chdir(getenv("HOME")); // sin argumentos -> ir a HOME
            } else if (chdir(parse_command[1].c_str()) != 0) {
                perror("cd");
            }
            continue; // no usar fork para cd
        }
        else if (parse_command[0] == "wc" && parse_command.size() == 1) {
            std::cerr << RED << "wc: debe especificar un archivo" << RESET << endl;
            continue; 
        }
        /*
            * Implementación del comando miprof
            * Modos:
            *  miprof ejec <comando> [args...] 
            *  miprof ejecsave <archivo> <comando> [args...]
            *  miprof ejecutar <maxTiempo> <comando> [args...]
            *  Reporta:
            *   - Tiempo real
            *   - Tiempo usuario
            *   - Tiempo sistema
            *   - Máxima memoria residente
            *  En modo ejecutar, si el proceso excede maxTiempo, se termina y se indica en el reporte
            *  En modo ejecsave, el reporte se guarda en el archivo especificado
        */
        else if(parse_command[0] == "miprof"){
            if(parse_command.size() < 3){
                std::cerr << RED << "miprof: se requieren al menos dos argumentos" << RESET << endl;
                continue;
            }
            string modo = parse_command[1];
            int idx_cmd = -1;
            int maxTiempo = 0;
            string archivo;

            if(modo == "ejec"){
                idx_cmd = 2;
            }else if(modo == "ejecsave"){
                if(parse_command.size() < 4){
                    std::cerr << RED << "Falta archivo.\n" << RESET;
                    continue;
                }
                archivo = parse_command[2];
                idx_cmd = 3;
            }else if (modo == "ejecutar"){
                if(parse_command.size() < 4){
                    std::cerr << RED << "Falta maxTiempo y comando.\n" << RESET;
                    continue;
                }
                maxTiempo = stoi(parse_command[2]);
                idx_cmd = 3;
            }else {
                std::cerr << RED << "miprof: modo no reconocido" << RESET <<endl;
                continue;
            }

            //construccion argv
            vector<char*> args;
            for (size_t i = idx_cmd; i < parse_command.size(); i++)
                args.push_back(const_cast<char*>(parse_command[i].c_str()));
            args.push_back(nullptr);

            timespec t0{}, t1{};
            clock_gettime(CLOCK_MONOTONIC, &t0); //toma tiempo inicio

            //timeout
            struct sigaction sa{};
            if(modo =="ejecutar"){
                sa.sa_handler = kill_child;
                sigemptyset(&sa.sa_mask);
                sa.sa_flags = SA_RESTART;
                sigaction(SIGALRM, &sa, nullptr);
                alarm(maxTiempo);
            }

            pid_t pid = fork();
            if (pid == 0) {
                execvp(args[0], args.data());
                perror("execvp");
                exit(EXIT_FAILURE);
            }else if (pid < 0) {
                perror("fork");
                continue;
            }
            g_child = pid;

            int status;
            rusage ru{};
            wait4(pid, &status, 0, &ru);
            if(modo == "ejecutar"){
                alarm(0);
            }

            //tiempo fin
            clock_gettime(CLOCK_MONOTONIC, &t1); 
            double tiempoReal = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9;
            double tiempoUsuario = ru.ru_utime.tv_sec + ru.ru_utime.tv_usec / 1e6;
            double tiempoSistema = ru.ru_stime.tv_sec + ru.ru_stime.tv_usec / 1e6;
            long maxMemoria = ru.ru_maxrss; 

            //crear reporte
            ostringstream output;
            output << "Comando:";
            for (size_t i = idx_cmd; i < parse_command.size(); i++){
                output << " " << parse_command[i];
            }
            output << "\nTiempo real: " << tiempoReal << " s"
                   << "\nTiempo usuario: " << tiempoUsuario << " s"
                   << "\nTiempo sistema: " << tiempoSistema << " s"
                   << "\nMax memoria: " << maxMemoria << " kB";

            if(modo == "ejecutar" && !(WIFEXITED(status))){
                output << "\nProceso terminado: Timeout (" << maxTiempo << " s)";
            }
            string reporte = output.str();
            //guardar reporte en archivo
            if(modo == "ejecsave"){
                ofstream out(archivo, ios::app);
                if(!out){
                    perror("miprof: error al abrir archivo");
                    continue;
                }
                out << "\n\n" << reporte << "\n\n";
            }
            //imprimir reporte
            std::cout << reporte << std::endl;
            continue;
        }
        /*
            -------------------------------------------------------------------------------------------------
                                                        Fin miprof
            -------------------------------------------------------------------------------------------------
        */

        //manejo de pipes
        vector<vector<string>> commands_pipe = separadoPorPipes(parse_command);
        int num_cmds = commands_pipe.size();

        if (num_cmds > 1) {
            vector<array<int, 2>> pipes(num_cmds - 1);
            for (int i = 0; i < num_cmds - 1; i++) {
                if (pipe(pipes[i].data()) == -1) {
                    perror("pipe");
                    exit(EXIT_FAILURE);
                }
            }
            for (int i = 0; i < num_cmds; i++) {
                pid_t pid = fork();
                if (pid == 0) {
                    if (i > 0) dup2(pipes[i-1][0], STDIN_FILENO);
                    if (i < num_cmds - 1) dup2(pipes[i][1], STDOUT_FILENO);
                    for (int j = 0; j < num_cmds - 1; j++) {
                        close(pipes[j][0]);
                        close(pipes[j][1]);
                    }

                    vector<char*> args;
                    for (auto &s : commands_pipe[i])
                        args.push_back(const_cast<char*>(s.c_str()));
                    args.push_back(nullptr);

                    execvp(args[0], args.data());
                    std::cerr << RED << args[0] << ": comando no encontrado" << RESET << endl;
                    exit(EXIT_FAILURE);
                } else if (pid < 0) {
                    perror("fork");
                    exit(EXIT_FAILURE);
                }
            }
            for (int i = 0; i < num_cmds - 1; i++) {
                close(pipes[i][0]);
                close(pipes[i][1]);
            }
            for (int i = 0; i < num_cmds; i++) wait(NULL);
            continue;
        }

        //comandos externos sin pipes
        pid_t pid = fork();
        if (pid == 0) {
            vector<char*> args;
            for (auto &s : parse_command)
                args.push_back(const_cast<char*>(s.c_str()));
            args.push_back(nullptr);

            execvp(args[0], args.data());
            std::cerr << RED << args[0] << ": comando no encontrado" << RESET << endl;
            exit(EXIT_FAILURE);
        }
        else if (pid > 0) {
            wait(NULL);
        }
        else {
            perror("fork");
        }
    }

    return 0;
}






