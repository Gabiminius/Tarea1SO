#include <iostream>
#include <string>
#include <sstream>
#include <unistd.h>
#include <pwd.h>
#include <sys/wait.h>
#include <vector>

#define RESET   "\033[0m"
#define GREEN   "\033[32m"  // color prompt
#define RED     "\033[31m"  // color errores

using namespace std;

const int num_commands = 4;
const string commands[num_commands] = {"ls", "cd", "wc", "exit"};

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

        if (!isCorrect) {
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

        //comandos externos (ls, wc)
        pid_t pid = fork();
        if (pid == 0) {
            // proceso hijo → ejecutar comando
            vector<char*> args;
            for (auto &s : parse_command) {
                args.push_back(const_cast<char*>(s.c_str()));
            }
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

