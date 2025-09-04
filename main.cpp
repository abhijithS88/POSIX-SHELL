#include "shell.h"
#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <cstring>
#include <vector>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <string.h>
#include <string>
#include <algorithm>
#include <sstream>
using namespace std;

string display;
string host;
vector<string>directories;
vector<string>all;
vector<string>cdfiles;
vector<string>history(20);
int history_ind = 0;
int fg_pid = 0;

int main(){
    signal(SIGINT, sigint_handler);
    signal(SIGTSTP, sigtstp_handler);
    all = getcommands();
    cdfiles = getcdfiles();
    rl_attempted_completion_function = my_completion;
    display+=getenv("USER");
    char h[256];
    gethostname(h,sizeof(h));
    host = h;
    display+="@";
    display+=host;
    display+=":";
    char h_dir[256];
    getcwd(h_dir,sizeof(h_dir));
    directories.push_back(h_dir);
    load_history_file();
    while(1){
        string prompt = display + getDirectory() + "> ";
        char* input = readline(prompt.c_str());
        if (!input) break; 
        char* line = strdup(input);
        if (*line){
            history[history_ind]=line;
            history_ind++;
            history_ind%=20;
            add_history(line);
            save_history_file();
        }
        vector<char*> args;
        char* cmd = strtok(line, ";");
        while (cmd) {
            args.push_back(cmd);
            cmd = strtok(NULL, ";");
        }
        for (char* c : args) {
            string sss = c;
            sss.erase(0, sss.find_first_not_of(" \t\n\r"));
            sss.erase(sss.find_last_not_of(" \t\n\r") + 1);
            if (sss == "exit") {
                cout << "Exiting shell...\n";
                free(input);
                return 0;
            }
            pipeline(c); 
            char dir[256]; 
            getcwd(dir, sizeof(dir));
            maintainDirectory(dir);
            cdfiles = getcdfiles();
        }
        free(input);
    }
    return 0;
}