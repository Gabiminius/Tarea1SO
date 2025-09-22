#include <iostream>
#include <string>
#include <sstream>
#include <unistd.h>
#include <pwd.h>
#include <sys/wait.h>
#include <vector>
#include <array> 

#define RESET   "\033[0m"
#define GREEN   "\033[32m"  // color prompt
#define RED     "\033[31m"  // color errores

using namespace std;

const int num_commands = 4;
const string commands[num_commands] = {"ls", "cd", "wc", "exit"};

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

        //parsear el comando 
        istringstream iss(command);
        vector<string> parse_command;
        while (iss >> word) parse_command.push_back(word);

        if (parse_command.empty()) continue;

        //verificar que el comando exista
        bool isCorrect = false;
        for (int i = 0; i < num_commands; i++) {
            if (parse_command[0] == commands[i]) {
                isCorrect = true;
                break;
            }
        }

        if (!isCorrect && parse_command[0] != "|") {
            string bad_command;
            for (size_t i = 0; i < parse_command.size(); i++) {
                bad_command += parse_command[i];
                if (i != parse_command.size() - 1) bad_command += " ";
            }
            cerr << RED << bad_command << ": comando no encontrado" << RESET << endl;
            continue;
        }

        //comandos internos 
        if (parse_command[0] == "exit") {
            cout << "Saliendo de la MIrkoshell..." << endl;
            exit(0);
        }
        else if (parse_command[0] == "cd") {
            if (parse_command.size() < 2) {
                chdir(getenv("HOME")); // sin argumentos -> ir aHOME
            } else if (chdir(parse_command[1].c_str()) != 0) {
                perror("cd");
            }
            continue; // no usar fork para cd
        }
        if (parse_command[0] == "wc" && parse_command.size() == 1) {
            cerr << RED << "wc: debe especificar un archivo" << RESET << endl;
            continue; // Volver al prompt sin ejecutar fork
        }

        // Manejo de pipes 

        // Si el comando tiene '|', lo dividimos y conectamos cada subcomando con pipes
        vector<vector<string>> commands_pipe = separadoPorPipes(parse_command);
        int num_cmds = commands_pipe.size();

        // Crear pipes
        if (num_cmds > 1) {
            vector<array<int, 2>> pipes(num_cmds - 1);
            for (int i = 0; i < num_cmds - 1; i++) {
                if (pipe(pipes[i].data()) == -1) {
                    perror("pipe");
                    exit(EXIT_FAILURE);
                }
            }
            // Crear un proceso hijo por cada subcomando
            for (int i = 0; i < num_cmds; i++) {
                pid_t pid = fork();
                if (pid == 0) {
                    if (i > 0) dup2(pipes[i-1][0], STDIN_FILENO);
                    if (i < num_cmds - 1) dup2(pipes[i][1], STDOUT_FILENO);
                    // Cerrar todos los pipes en el hijo
                    for (int j = 0; j < num_cmds - 1; j++) {
                        close(pipes[j][0]);
                        close(pipes[j][1]);
                    }

                    vector<char*> args;
                    for (auto &s : commands_pipe[i])
                        args.push_back(const_cast<char*>(s.c_str()));
                    args.push_back(nullptr);

                    execvp(args[0], args.data());
                    perror("execvp"); // error al ejecutar comando
                    exit(EXIT_FAILURE);
                } else if (pid < 0) {
                    perror("fork"); // error al crear hijo
                    exit(EXIT_FAILURE);
                }
            }
            // Padre cierra todos los pipes y espera a todos los hijos
            for (int i = 0; i < num_cmds - 1; i++) {
                close(pipes[i][0]);
                close(pipes[i][1]);
            }

            for (int i = 0; i < num_cmds; i++)
                wait(NULL);

            continue;
        }

        //comandos externos sin pipe (ls, wc)
        pid_t pid = fork();
        if (pid == 0) {
            // proceso hijo → ejecutar comando
            vector<char*> args;
            for (auto &s : parse_command)
                args.push_back(const_cast<char*>(s.c_str()));
            args.push_back(nullptr);

            if (execvp(args[0], args.data()) < 0) {
                cerr << RED << parse_command[0] << ": error al ejecutar" << RESET << endl;
                exit(EXIT_FAILURE);
            }
        }
        else if (pid > 0) {
            // proceso padre espera al hijo
            wait(NULL);
        }
        else {
            perror("fork");
        }
    }

    return 0;
}


