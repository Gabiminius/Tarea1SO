#include <iostream>
#include <string>
#include <sstream>
#include <unistd.h>
#include <pwd.h>
#include <sys/utsname.h>
#include <vector>

#define RESET   "\033[0m"
#define GREEN   "\033[32m"  // color prompt

using namespace std; 

const int num_commands = 4;
const string commands[num_commands] = {"ls", "cd", "wc", "exit"};
const string commands_args[num_commands] = {{}, {}, {}, {}};

int main() {
    while (true) {
        struct passwd* pw = getpwuid(getuid());
        string username = pw->pw_name;
        string dir = pw->pw_dir;   // pw_dir entrega el directorio home del usuario
        string command, word;
        int command_index;
        char hostname[1024];
        gethostname(hostname, sizeof(hostname));

        cout << GREEN << username << "@" << hostname << ":" << dir << "$ " << RESET;
        getline(cin, command);

        // Parsear el comando ingresado y separarlo en comando + argumentos
        istringstream iss(command);
        vector<string> parse_command;

        while (iss >> word) {
            parse_command.push_back(word);
        }

        if (parse_command.empty()) continue;

        // Verificar que el comando exista
        bool isCorrect = false;
        for (int i = 0; i < num_commands; i++) {
            if (parse_command[0] == commands[i]) {
                command_index = i;
                isCorrect = true;
            }
        }

        // Si el comando no existe → mensaje de error
        if (!isCorrect) {
            string bad_command;
            for (size_t i = 0; i < parse_command.size(); i++) {
                bad_command += parse_command[i];
                if (i != parse_command.size() - 1) bad_command += " ";
            }
            cerr << bad_command << ": no se encontró la orden" << endl;
            continue;
        }

        // Implementación de comandos
        if (parse_command[0] == "exit") {
            exit(0);
        } else if (parse_command[0] == "ls") {
            cout << "[implementar fork + execvp para ejecutar 'ls']" << endl;
        }
    }
}

