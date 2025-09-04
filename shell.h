#ifndef SHELL_H
#define SHELL_H

#include <iostream>
#include <vector>
#include <string>
using namespace std;


extern string display;
extern string host;
extern vector<string> directories;
extern vector<string> all;
extern vector<string> cdfiles;
extern vector<string> history;
extern int history_ind;
extern int fg_pid;


vector<string> getcommands();
vector<string> getcdfiles();
string getDirectory();
void maintainDirectory(char dir[]);


void load_history_file();
void save_history_file();
void history_print(char* &rem);


void cd(char* &rem);
void pwd(char* &rem);
void echo(char* &rem);
void ls(char* &rem);
void pinfo(char* &rem);
void search(char* &rem);


void sigint_handler(int sig);
void sigtstp_handler(int sig);


char* my_generator(const char* text, int state);
char** my_completion(const char* text, int start, int end);


void process(char* &cmd);
void run(char* cmd);
void pipeline(char* cmd);

#endif
