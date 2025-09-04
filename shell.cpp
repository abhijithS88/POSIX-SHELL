// shell.cpp
#include "shell.h"
#include <iostream>
#include <string>
#include <cstring>
#include <string.h>
#include <sstream>
#include <vector>
#include <algorithm>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pwd.h>
#include <grp.h>
#include <ctime>
#include <dirent.h>
#include <readline/readline.h>
#include <readline/history.h>
using namespace std;

vector<string> getcommands() {
    vector<string> cmds;
    const char* path_env = getenv("PATH");
    if (!path_env) return {};

    string path_str(path_env);
    stringstream ss(path_str);
    string dir;

    while (getline(ss, dir, ':')) {
        DIR* d = opendir(dir.c_str());
        if (!d) continue;

        struct dirent* entry;
        while ((entry = readdir(d)) != NULL) {
            string name = entry->d_name;
            string full_path = dir + "/" + name;
            struct stat st;
            if (stat(full_path.c_str(), &st) == 0 && (st.st_mode & S_IXUSR)) {
                bool exists = false;
                for (auto &cmd : cmds) {
                    if (cmd == name) {
                        exists = true;
                        break;
                    }
                }
                if (!exists) cmds.push_back(name);
            }
        }
        closedir(d);
    }

    return cmds;
}

vector<string> getcdfiles() {
    vector<string> files;
    DIR* d = opendir(".");
    if (!d) return files;

    struct dirent* entry;
    while ((entry = readdir(d)) != NULL) {
        files.push_back(entry->d_name);
    }
    closedir(d);
    return files;
}

char* my_generator(const char* text, int state) {
    static vector<string> matches;
    static size_t list_index;
    static size_t len;

    if (state == 0) {
        matches.clear();
        list_index = 0;
        len = strlen(text);

        vector<string> all1 = all;
        all1.insert(all1.end(), cdfiles.begin(), cdfiles.end());

        for (auto &s : all1) {
            if (s.compare(0, len, text) == 0) {
                matches.push_back(s);
            }
        }
        sort(matches.begin(), matches.end());
    }

    if (list_index < matches.size()) {
        return strdup(matches[list_index++].c_str());
    }
    return nullptr;
}

char** my_completion(const char* text, int start, int end) {
    (void)start; (void)end;
    return rl_completion_matches(text, my_generator);
}

void sigint_handler(int sig) {
    if(fg_pid != 0) {
        kill(fg_pid, SIGINT);
        cout << "\nProcess " << fg_pid << " terminated\n";
        fg_pid = 0;
    }
}

void sigtstp_handler(int sig) {
    if(fg_pid != 0) {
        kill(fg_pid, SIGTSTP);
        cout << "\nProcess " << fg_pid << " stopped\n";
        // Add fg_pid to background jobs list here
        fg_pid = 0; // no foreground now
    }
}

void load_history_file() {
    string file = directories[0] + "/history.txt";
    FILE* f = fopen(file.c_str(), "r");
    if (!f) return;
    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        int len = strlen(line);
        if (len > 0 && line[len-1] == '\n') line[len-1] = '\0';
        history[history_ind] = line; 
        history_ind = (history_ind + 1) % 20;
        add_history(line); 
    }
    fclose(f);
}

void save_history_file() {
    string file = directories[0] + "/history.txt";
    FILE* f = fopen(file.c_str(), "w");
    if (!f) return;
    int cnt = 0;
    int i = history_ind;
    while (cnt < 20) {
        if (!history[i].empty()) fprintf(f, "%s\n", history[i].c_str());
        i = (i + 1) % 20;
        cnt++;
    }
    fclose(f);
}

string getDirectory(){
    string s = "~";
    int i = 0,len = directories[0].length(),sz = directories.size();
    while(i<len){
        if(directories[0][i]==directories[sz-1][i])i++;
        else break;
    }
    if(i==len){
        len = directories[sz-1].length();
        while(i<len){
            s += directories[sz-1][i];
            i++;
        }
    }
    else{
        if(s.length())s.pop_back();
        s = directories[sz-1];
    }
    return s;
}

void maintainDirectory(char dir[]){
    int sz = directories.size();
    string str1 = directories[sz-1];
    if(directories.size()>1)directories.pop_back();
    if(directories.size()>1)directories.pop_back();
    directories.push_back(str1);
    directories.push_back(dir);
}

void cd(char* &rem){
    vector<char*>args;
    char* s = strtok(rem," \t");
    while(s){
        args.push_back(s);
        s = strtok(NULL," \t");
    }
    if(args.size()>1){
        cout<<"command not supported"<<endl;
        return;
    }
    string str="";
    if(args.size()==0){
        str = directories[0];
    }
    else if(strcmp(args[0],"~")==0){
        str = directories[0];
    }
    else if(strcmp(args[0],".")==0){
        int sz = (int)directories.size();
        str = directories[sz-1];
    }
    else if(strcmp(args[0],"-")==0){
        int sz = (int)directories.size();
        str = directories[sz-2];
    }
    else if(strcmp(args[0],"..")==0){
        int sz = (int)directories.size();
        str = directories[sz-1];
        sz = str.length();
        for(int i=sz-1;i>=0;i--){
            if(str[i]=='/'){
                str.pop_back();
                break;
            }
            str.pop_back();
        }
    }
    else{
        int sz = (int)directories.size();
        str = directories[sz-1];
        str+='/';
        str+=args[0];
    }
    if(chdir(str.c_str()) != 0) {
        perror("cd error");
    }
}

string trim(const string &s) {
    int start = 0, end = (int)s.size() - 1;
    while (start <= end && isspace(s[start])) start++;
    while (end >= start && isspace(s[end])) end--;
    return s.substr(start, end - start + 1);
}

void pwd(char* &rem){
    string out = "";
    bool add = false;
    vector<char*> args;
    char* s = strtok(rem," \t");
    while(s){
        if(strcmp(s, ">") == 0){
            s = strtok(NULL," \t");
            if(strlen(s)){
                out = s;
                add = false;
            }
        }
        else if(strcmp(s, ">>") == 0){
            s = strtok(NULL," \t");
            if(strlen(s)){
                out = s;
                add = true;
            }
        }
        else{
            args.push_back(s);
        }
        s = strtok(NULL," \t");
    }
    if(args.size()>0){
        cout<<"command not supported"<<endl;
        return ;
    }
    int stdout = dup(STDOUT_FILENO);
    if(out.length()){
        int fd;
        if(add){
            fd = open(out.c_str(),O_WRONLY|O_CREAT|O_APPEND, 0644);
        }
        else fd =  open(out.c_str(),O_WRONLY|O_CREAT|O_TRUNC, 0644);
        if(fd<0){
            perror("open sys call");
            exit(0);
        }
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }
    int sz = (int)directories.size();
    cout<<directories[sz-1];
    cout<<'\n';
    dup2(stdout,STDOUT_FILENO);
    close(stdout);
}

void echo(char * &rem){
    char* temp = strdup(rem);
    string s = temp;
    int add = 0, append = 0;
    int n = s.length();
    for(int i=0;i<n;i++){
        if(s[i]=='>'){
            if(i+1<n){
                if(s[i+1]=='>'){
                    append++;
                    i++;
                }
                else add++;
            }
            else add++;
        }
    }
    if(add+append>1){
        cout<<"Command not supported"<<endl;
        return;
    }
    if(add+append==0){
        cout<<rem<<endl;
    }
    else{
        string out = "";
        add = append = 0;
        int n = s.length();
        string ans = "";
        for(int i=0;i<n;i++){
            if(add or append){
                int j=i;
                while(j<n){
                    out+=s[j];
                    j++;
                }
                i=n-1;
            }
            else if(s[i]=='>'){
                if(i+1<n){
                    if(s[i+1]=='>'){
                        append++;
                        i++;
                    }
                    else add++;
                }
                else add++;
            }
            else{
                ans += s[i]; 
            }
        }
        out = trim(out);
        int stdout = dup(STDOUT_FILENO);
        if(out.length()){
            int fd;
            if(append){
                fd = open(out.c_str(),O_WRONLY|O_CREAT|O_APPEND, 0644);
            }
            else fd =  open(out.c_str(),O_WRONLY|O_CREAT|O_TRUNC, 0644);
            if(fd<0){
                perror("open sys call");
                exit(0);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }
        cout<<ans<<endl;
        dup2(stdout,STDOUT_FILENO);
        close(stdout);
    }
}

void ls_dir(string &path , string out , bool add){
    struct stat st;
    if(stat(path.c_str(), &st) == 0) {
        if(S_ISDIR(st.st_mode)) {

        } 
        else{
            int stdout = dup(STDOUT_FILENO);
            if(out.length()){
                int fd;
                if(add){
                    fd = open(out.c_str(),O_WRONLY|O_CREAT|O_APPEND, 0644);
                }
                else fd =  open(out.c_str(),O_WRONLY|O_CREAT|O_TRUNC, 0644);
                if(fd<0){
                    perror("open sys call");
                    exit(0);
                }
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }
            int sz = path.size();
            string ans = "";
            for(int i = sz-1;i>=0;i--){
                if(path[i]=='/')break;
                ans+=path[i];
            }
            reverse(ans.begin(),ans.end());
            cout<<ans<<endl;
            dup2(stdout,STDOUT_FILENO);
            close(stdout);
            return;
        }
    }
    DIR* dir = opendir(path.c_str());
    if(dir==NULL){
        perror("opendir failed");
        return;
    }
    int stdout = dup(STDOUT_FILENO);
    if(out.length()){
        int fd;
        if(add){
            fd = open(out.c_str(),O_WRONLY|O_CREAT|O_APPEND, 0644);
        }
        else fd =  open(out.c_str(),O_WRONLY|O_CREAT|O_TRUNC, 0644);
        if(fd<0){
            perror("open sys call");
            exit(0);
        }
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }
    struct dirent* file;
    while(1){
        file = readdir(dir);
        if(file==NULL)break;
        if(file->d_name[0]=='.')continue;
        cout<<file->d_name<<"\n";
    }
    dup2(stdout,STDOUT_FILENO);
    close(stdout);
    closedir(dir);
}

void ls_a(string &path , string out , bool add) {
    struct stat st;
    if(stat(path.c_str(), &st) == 0) {
        if(S_ISDIR(st.st_mode)) {

        } 
        else{
            int stdout = dup(STDOUT_FILENO);
            if(out.length()){
                int fd;
                if(add){
                    fd = open(out.c_str(),O_WRONLY|O_CREAT|O_APPEND, 0644);
                }
                else fd =  open(out.c_str(),O_WRONLY|O_CREAT|O_TRUNC, 0644);
                if(fd<0){
                    perror("open sys call");
                    exit(0);
                }
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }
            int sz = path.size();
            string ans = "";
            for(int i = sz-1;i>=0;i--){
                if(path[i]=='/')break;
                ans+=path[i];
            }
            reverse(ans.begin(),ans.end());
            cout<<ans<<endl;
            dup2(stdout,STDOUT_FILENO);
            close(stdout);
            return;
        }
    }
    DIR *dir = opendir(path.c_str());
    if(dir==NULL) {
        perror("opendir error");
        return;
    }
    int stdout = dup(STDOUT_FILENO);
    if(out.length()){
        int fd;
        if(add){
            fd = open(out.c_str(),O_WRONLY|O_CREAT|O_APPEND, 0644);
        }
        else fd =  open(out.c_str(),O_WRONLY|O_CREAT|O_TRUNC, 0644);
        if(fd<0){
            perror("open sys call");
            exit(0);
        }
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }
    struct dirent* file;
    while(1){
        file = readdir(dir);
        if(file==NULL)break;
        cout<<file->d_name<<"\n";
    }
    closedir(dir);
    dup2(stdout,STDOUT_FILENO);
    close(stdout);
}

void ls_l(string &path, string out, bool add) {
    struct stat st;
    
    if(stat(path.c_str(), &st) != 0) {
        perror(("ls: cannot access " + path).c_str());
        return;
    }
    if(!S_ISDIR(st.st_mode)) {
        int stdout = dup(STDOUT_FILENO);
        if(out.length()){
            int fd;
            if(add){
                fd = open(out.c_str(),O_WRONLY|O_CREAT|O_APPEND, 0644);
            }
            else fd =  open(out.c_str(),O_WRONLY|O_CREAT|O_TRUNC, 0644);
            if(fd<0){
                perror("open sys call");
                exit(0);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }
        cout << ((S_ISDIR(st.st_mode)) ? 'd' : '-');
        cout << ((st.st_mode & S_IRUSR) ? 'r' : '-');
        cout << ((st.st_mode & S_IWUSR) ? 'w' : '-');
        cout << ((st.st_mode & S_IXUSR) ? 'x' : '-');
        cout << ((st.st_mode & S_IRGRP) ? 'r' : '-');
        cout << ((st.st_mode & S_IWGRP) ? 'w' : '-');
        cout << ((st.st_mode & S_IXGRP) ? 'x' : '-');
        cout << ((st.st_mode & S_IROTH) ? 'r' : '-');
        cout << ((st.st_mode & S_IWOTH) ? 'w' : '-');
        cout << ((st.st_mode & S_IXOTH) ? 'x' : '-');
        cout << " " << st.st_nlink;
        struct passwd *pw = getpwuid(st.st_uid);
        struct group  *gr = getgrgid(st.st_gid);
        cout << " " << (pw ? pw->pw_name : to_string(st.st_uid));
        cout << " " << (gr ? gr->gr_name : to_string(st.st_gid));
        size_t sz = st.st_size;
        int val = (long long)sz;
        int cnt = 0;
        while(val){
            val/=10;
            cnt++;
        }
        for(int i=0;i<10-cnt;i++){
            cout<<" ";
        }
        cout<<sz;
        char buf[64];
        strftime(buf, sizeof(buf), "%b %d %H:%M", localtime(&st.st_mtime));
        cout << " " << buf;
        cout << " ";
        int sz1 = path.size();
        string ans = "";
        for(int i = sz1-1;i>=0;i--){
            if(path[i]=='/')break;
            ans+=path[i];
        }
        reverse(ans.begin(),ans.end());
        cout<<ans<<endl;
        dup2(stdout,STDOUT_FILENO);
        close(stdout);
        return;
    }

    DIR* dir = opendir(path.c_str());
    if(!dir) {
        perror(("ls: cannot open directory " + path).c_str());
        return;
    }
    int stdout = dup(STDOUT_FILENO);
    if(out.length()){
        int fd;
        if(add){
            fd = open(out.c_str(),O_WRONLY|O_CREAT|O_APPEND, 0644);
        }
        else fd =  open(out.c_str(),O_WRONLY|O_CREAT|O_TRUNC, 0644);
        if(fd<0){
            perror("open sys call");
            exit(0);
        }
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }
    struct dirent* entry;
    while((entry = readdir(dir)) != nullptr) {
        string file_name = entry->d_name;
        if(file_name[0] == '.') continue;
        string full_path = path + "/" + file_name;
        if(stat(full_path.c_str(), &st) != 0) continue;
        cout << ((S_ISDIR(st.st_mode)) ? 'd' : '-');
        cout << ((st.st_mode & S_IRUSR) ? 'r' : '-');
        cout << ((st.st_mode & S_IWUSR) ? 'w' : '-');
        cout << ((st.st_mode & S_IXUSR) ? 'x' : '-');
        cout << ((st.st_mode & S_IRGRP) ? 'r' : '-');
        cout << ((st.st_mode & S_IWGRP) ? 'w' : '-');
        cout << ((st.st_mode & S_IXGRP) ? 'x' : '-');
        cout << ((st.st_mode & S_IROTH) ? 'r' : '-');
        cout << ((st.st_mode & S_IWOTH) ? 'w' : '-');
        cout << ((st.st_mode & S_IXOTH) ? 'x' : '-');
        cout << " " << st.st_nlink;
        struct passwd *pw = getpwuid(st.st_uid);
        struct group  *gr = getgrgid(st.st_gid);
        cout << " " << (pw ? pw->pw_name : to_string(st.st_uid));
        cout << " " << (gr ? gr->gr_name : to_string(st.st_gid));
        size_t sz = st.st_size;
        int val = (long long)sz;
        int cnt = 0;
        while(val){
            val/=10;
            cnt++;
        }
        for(int i=0;i<10-cnt;i++){
            cout<<" ";
        }
        cout<<sz;
        char buf[64];
        strftime(buf, sizeof(buf), "%b %d %H:%M", localtime(&st.st_mtime));
        cout << " " << buf;
        cout << " " << file_name << "\n";
    }
    dup2(stdout,STDOUT_FILENO);
    close(stdout);
    closedir(dir);
}

void ls_al(string &path, string out, bool add) {
    struct stat st;
    
    if(stat(path.c_str(), &st) != 0) {
        perror(("ls: cannot access " + path).c_str());
        return;
    }
    if(!S_ISDIR(st.st_mode)) {
        int stdout = dup(STDOUT_FILENO);
        if(out.length()){
            int fd;
            if(add){
                fd = open(out.c_str(),O_WRONLY|O_CREAT|O_APPEND, 0644);
            }
            else fd =  open(out.c_str(),O_WRONLY|O_CREAT|O_TRUNC, 0644);
            if(fd<0){
                perror("open sys call");
                exit(0);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }
        cout << ((S_ISDIR(st.st_mode)) ? 'd' : '-');
        cout << ((st.st_mode & S_IRUSR) ? 'r' : '-');
        cout << ((st.st_mode & S_IWUSR) ? 'w' : '-');
        cout << ((st.st_mode & S_IXUSR) ? 'x' : '-');
        cout << ((st.st_mode & S_IRGRP) ? 'r' : '-');
        cout << ((st.st_mode & S_IWGRP) ? 'w' : '-');
        cout << ((st.st_mode & S_IXGRP) ? 'x' : '-');
        cout << ((st.st_mode & S_IROTH) ? 'r' : '-');
        cout << ((st.st_mode & S_IWOTH) ? 'w' : '-');
        cout << ((st.st_mode & S_IXOTH) ? 'x' : '-');
        cout << " " << st.st_nlink;
        struct passwd *pw = getpwuid(st.st_uid);
        struct group  *gr = getgrgid(st.st_gid);
        cout << " " << (pw ? pw->pw_name : to_string(st.st_uid));
        cout << " " << (gr ? gr->gr_name : to_string(st.st_gid));
        size_t sz = st.st_size;
        int val = (long long)sz;
        int cnt = 0;
        while(val){
            val/=10;
            cnt++;
        }
        for(int i=0;i<10-cnt;i++){
            cout<<" ";
        }
        cout<<sz;
        char buf[64];
        strftime(buf, sizeof(buf), "%b %d %H:%M", localtime(&st.st_mtime));
        cout << " " << buf;
        cout << " ";
        int sz1 = path.size();
        string ans = "";
        for(int i = sz1-1;i>=0;i--){
            if(path[i]=='/')break;
            ans+=path[i];
        }
        reverse(ans.begin(),ans.end());
        cout<<ans<<endl;
        dup2(stdout,STDOUT_FILENO);
        close(stdout);
        return;
    }
    
    DIR* dir = opendir(path.c_str());
    if(!dir) {
        perror(("ls: cannot open directory " + path).c_str());
        return;
    }
    int stdout = dup(STDOUT_FILENO);
    if(out.length()){
        int fd;
        if(add){
            fd = open(out.c_str(),O_WRONLY|O_CREAT|O_APPEND, 0644);
        }
        else fd =  open(out.c_str(),O_WRONLY|O_CREAT|O_TRUNC, 0644);
        if(fd<0){
            perror("open sys call");
            exit(0);
        }
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }
    struct dirent* entry;
    while((entry = readdir(dir)) != nullptr) {
        string file_name = entry->d_name;
        string full_path = path + "/" + file_name;
        if(stat(full_path.c_str(), &st) != 0) continue;
        cout << ((S_ISDIR(st.st_mode)) ? 'd' : '-');
        cout << ((st.st_mode & S_IRUSR) ? 'r' : '-');
        cout << ((st.st_mode & S_IWUSR) ? 'w' : '-');
        cout << ((st.st_mode & S_IXUSR) ? 'x' : '-');
        cout << ((st.st_mode & S_IRGRP) ? 'r' : '-');
        cout << ((st.st_mode & S_IWGRP) ? 'w' : '-');
        cout << ((st.st_mode & S_IXGRP) ? 'x' : '-');
        cout << ((st.st_mode & S_IROTH) ? 'r' : '-');
        cout << ((st.st_mode & S_IWOTH) ? 'w' : '-');
        cout << ((st.st_mode & S_IXOTH) ? 'x' : '-');
        cout << " " << st.st_nlink;
        struct passwd *pw = getpwuid(st.st_uid);
        struct group  *gr = getgrgid(st.st_gid);
        cout << " " << (pw ? pw->pw_name : to_string(st.st_uid));
        cout << " " << (gr ? gr->gr_name : to_string(st.st_gid));
        size_t sz = st.st_size;
        int val = (long long)sz;
        int cnt = 0;
        while(val){
            val/=10;
            cnt++;
        }
        for(int i=0;i<10-cnt;i++){
            cout<<" ";
        }
        cout<<sz;
        char buf[64];
        strftime(buf, sizeof(buf), "%b %d %H:%M", localtime(&st.st_mtime));
        cout << " " << buf;
        cout << " " << file_name << "\n";
    }
    dup2(stdout,STDOUT_FILENO);
    close(stdout);
    closedir(dir);
}

void ls(char* &rem){
    string out = "";
    bool add = false;
    vector<char*> args;
    char* s = strtok(rem," \t");
    while(s){
        if(strcmp(s, ">") == 0){
            s = strtok(NULL," \t");
            if(strlen(s)){
                out = s;
                add = false;
            }
        }
        else if(strcmp(s, ">>") == 0){
            s = strtok(NULL," \t");
            if(strlen(s)){
                out = s;
                add = true;
            }
        }
        else{
            args.push_back(s);
        }
        s = strtok(NULL," \t");
    }
    int a=0,l=0;
    vector<char*>dirs;
    for(char * cc : args){
        if(strcmp(cc,"-al")==0 or strcmp(cc,"-la")==0){
            a++;
            l++;
        }
        else if(strcmp(cc,"-l")==0){
            l++;
        }
        else if(strcmp(cc,"-a")==0){
            a++;
        }
        else dirs.push_back(cc);
    }
    string flag = "";
    if(a)flag+="a";
    if(l)flag+="l";
    if(dirs.size()==0){
        int sz = directories.size();
        string path = directories[sz-1];
        if(flag=="al"){
            ls_al(path,out,add);
        }
        else if(flag=="a"){
            ls_a(path,out,add);
        }
        else if(flag=="l"){
            ls_l(path,out,add);
        }
        else ls_dir(path,out,add);
        return;
    }
    for(char * cc : dirs){
        if(strcmp(cc,"..")==0){
            string path;
            int sz = directories.size();
            path = directories[sz-1];
            sz = path.length();
            for(int i=sz-1;i>=0;i--){
                if(path[i]=='/'){
                    path.pop_back();
                    break;
                }
                path.pop_back();
            }
            if(flag=="al"){
                ls_al(path,out,add);
            }
            else if(flag=="a"){
                ls_a(path,out,add);
            }
            else if(flag=="l"){
                ls_l(path,out,add);
            }
            else ls_dir(path,out,add);
        }
        else if(strcmp(cc,".")==0){
            string path;
            int sz = directories.size();
            path = directories[sz-1];
            if(flag=="al"){
                ls_al(path,out,add);
            }
            else if(flag=="a"){
                ls_a(path,out,add);
            }
            else if(flag=="l"){
                ls_l(path,out,add);
            }
            else ls_dir(path,out,add);
        }
        else if(strcmp(cc,"~")==0){
            string path;
            path = directories[0];
            if(flag=="al"){
                ls_al(path,out,add);
            }
            else if(flag=="a"){
                ls_a(path,out,add);
            }
            else if(flag=="l"){
                ls_l(path,out,add);
            }
            else ls_dir(path,out,add);
        }
        else{
            string path;
            int sz = directories.size();
            path = directories[sz-1];
            path += "/";
            path+=cc;
            if(flag=="al"){
                ls_al(path,out,add);
            }
            else if(flag=="a"){
                ls_a(path,out,add);
            }
            else if(flag=="l"){
                ls_l(path,out,add);
            }
            else ls_dir(path,out,add);
        }
    }
}

void pinfo(char* &rem){
    string out = "";
    bool add = false;
    vector<char*> args;
    char* s = strtok(rem," \t");
    while(s){
        if(strcmp(s, ">") == 0){
            s = strtok(NULL," \t");
            if(strlen(s)){
                out = s;
                add = false;
            }
        }
        else if(strcmp(s, ">>") == 0){
            s = strtok(NULL," \t");
            if(strlen(s)){
                out = s;
                add = true;
            }
        }
        else{
            args.push_back(s);
        }
        s = strtok(NULL," \t");
    }
    if(args.size()>1){
        cout<<"Not supported"<<endl;
        return;
    }
    int pid;
    if(args.size()==0){
        pid = getpid();
    }
    else{
        pid = atoi(args[0]);
    }
    char path[256],buf[5000];
    sprintf(path, "/proc/%d/stat", pid);
    int fd = open(path, O_RDONLY);
    if (fd < 0) { perror("pinfo stat"); return; }
    int n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0) return;
    buf[n] = '\0';
    string status="",memory="",tgpid="";
    int cnt=0;
    for(int i=0;i<n;i++){
        int j=i;
        cnt++;
        while(j<n){
            if(buf[j]==' ')break;
            else{
                if(cnt==3)status+=buf[j];
                if(cnt==8)tgpid+=buf[j];
                if(cnt==23)memory+=buf[j];
            }
            j++;
        }
        i=j;
    }
    sprintf(path, "/proc/%d/exe", pid);
    char exe[1024];
    int len = readlink(path, exe, sizeof(exe) - 1);
    if (len != -1) exe[len] = '\0';
    else strcpy(exe, "N/A");

    if(stoi(tgpid)!=pid){
        status+="+";
    }
    int stdout = dup(STDOUT_FILENO);
    if(out.length()){
        int fd;
        if(add){
            fd = open(out.c_str(),O_WRONLY|O_CREAT|O_APPEND, 0644);
        }
        else fd =  open(out.c_str(),O_WRONLY|O_CREAT|O_TRUNC, 0644);
        if(fd<0){
            perror("open sys call");
            exit(0);
        }
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }
    cout<<"PID -- "<<pid<<endl;
    cout<<"Process Status -- "<<status<<endl;
    cout<<"memory -- "<<memory<<" {Virtual Memory}"<<endl;
    cout<<"Executable Path -- "<<exe<<endl;
    dup2(stdout,STDOUT_FILENO);
    close(stdout);
}

bool search_recur(string path, char* &file1){
    DIR* dir = opendir(path.c_str());
    if(dir==NULL){
        // perror("opendir failed");
        return false;
    }
    struct dirent* file;
    bool ans = false;
    while(1){
        file = readdir(dir);
        if(file==NULL)break;
        if(file->d_name[0]=='.')continue;
        if(strcmp(file->d_name,file1)==0){
            closedir(dir);
            return true;
        }
        string new_path = path;
        new_path +="/";
        new_path +=file->d_name;
        struct stat st;
        if(stat(new_path.c_str(),&st)!=0){
            // perror("stat sys call error");
            return false;
        }
        if(S_ISDIR(st.st_mode)){
            ans|=search_recur(new_path,file1);
        }
    }
    closedir(dir);
    return ans;
}

void search(char* &rem){
    vector<char*>args;
    char* s = strtok(rem," \t");
    while(s){
        args.push_back(s);
        s = strtok(NULL," \t");
    }
    if(args.size()>1){
        cout<<"Not supported"<<endl;
        return;
    }
    if(args.size()==0){
        cout<<"Input file name needed"<<endl;
        return;
    }
    int sz = (int)directories.size();
    cout<<(search_recur(directories[sz-1],args[0])?"True":"False")<<endl;
}

void history_print(char* &rem){
    string out = "";
    bool add = false;
    vector<char*> args;
    char* s = strtok(rem," \t");
    while(s){
        if(strcmp(s, ">") == 0){
            s = strtok(NULL," \t");
            if(strlen(s)){
                out = s;
                add = false;
            }
        }
        else if(strcmp(s, ">>") == 0){
            s = strtok(NULL," \t");
            if(strlen(s)){
                out = s;
                add = true;
            }
        }
        else{
            args.push_back(s);
        }
        s = strtok(NULL," \t");
    }
    if(args.size()>1){
        cout<<"Not supported"<<endl;
        return;
    }
    int n;
    if(args.size()==0){
        n = 10;
    }
    else{
        n= atoi(args[0]);
    }
    int stdout = dup(STDOUT_FILENO);
    if(out.length()){
        int fd;
        if(add){
            fd = open(out.c_str(),O_WRONLY|O_CREAT|O_APPEND, 0644);
        }
        else fd =  open(out.c_str(),O_WRONLY|O_CREAT|O_TRUNC, 0644);
        if(fd<0){
            perror("open sys call");
            exit(0);
        }
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }
    int ind = history_ind-1;
    int cnt = 0;
    vector<string>ans;
    while(n-- and cnt<20){
        if(history[ind].size())ans.push_back(history[ind]);
        ind=(ind-1+20)%20;
        cnt++;
    }
    reverse(ans.begin(),ans.end());
    for(string s:ans){
        cout<<s<<endl;
    }
    dup2(stdout,STDOUT_FILENO);
    close(stdout);
}

void process(char* &cmd){
    char* first = strtok(cmd," \t");
    if(strcmp(first,"cd")==0){
        char* rem = strtok(NULL,"");
        cd(rem);
    }
    if(strcmp(first,"pwd")==0){
        char* rem = strtok(NULL,"");
        pwd(rem);
    }
    if(strcmp(first,"echo")==0){
        char* rem = strtok(NULL,"");
        echo(rem);
    }
    if(strcmp(first,"ls")==0){
        char* rem = strtok(NULL,"");
        ls(rem);
    }
    if(strcmp(first,"pinfo")==0){
        char* rem = strtok(NULL,"");
        pinfo(rem);
    }
    if(strcmp(first,"search")==0){
        char* rem = strtok(NULL,"");
        search(rem);
    }
    if(strcmp(first,"history")==0){
        char* rem = strtok(NULL,"");
        history_print(rem);
    }
}

void run(char* cmd){
    while(cmd[0]==' ' or cmd[0]=='\t'){
        cmd++;
    }
    int length = strlen(cmd);
    for(int i=length-1;i>=0;i--){
        if(cmd[i]==' ' or cmd[i]=='\n' or cmd[i]=='\t'){
            cmd[i]='\0';
        }
        else break;
    }
    length = strlen(cmd);
    if(length==0)return;
    bool bg = false;
    if(cmd[length-1]=='&'){
        bg = true;
        cmd[length-1]='\0';
        for(int i=length-2;i>=0;i--){
            if(cmd[i]==' ' or cmd[i]=='\n' or cmd[i]=='\t'){
                cmd[i]='\0';
            }
            else break;
        }
    }
    char* temp = strdup(cmd);
    char* first = strtok(temp," \t");
    if(strcmp(first,"cd")==0 or strcmp(first,"pwd")==0 or 
       strcmp(first,"echo")==0 or strcmp(first,"ls")==0 or
       strcmp(first,"pinfo")==0 or strcmp(first,"search")==0
        or strcmp(first,"history")==0){
        free(temp);
        process(cmd);
        return;
    }
    free(temp);
    string in = "", out = "";
    bool add = false;
    vector<char*> args;
    char* s = strtok(cmd," \t");
    while(s){
        if(strcmp(s, "<") == 0){
            s = strtok(NULL," \t");
            if(strlen(s)){
                in = s;
            } 
        }
        else if(strcmp(s, ">") == 0){
            s = strtok(NULL," \t");
            if(strlen(s)){
                out = s;
                add = false;
            }
        }
        else if(strcmp(s, ">>") == 0){
            s = strtok(NULL," \t");
            if(strlen(s)){
                out = s;
                add = true;
            }
        }
        else{
            args.push_back(s);
        }
        s = strtok(NULL," \t");
    }
    args.push_back(NULL);
    pid_t pid = fork();
    if(pid==0){
        if(in.length()){
            int fd = open(in.c_str(),O_RDONLY);
            if(fd<0){
                perror("open sys call");
                exit(0);
            }
            dup2(fd, STDIN_FILENO);
            close(fd);
        }
        if(out.length()){
            int fd;
            if(add){
                fd = open(out.c_str(),O_WRONLY|O_CREAT|O_APPEND, 0644);
            }
            else fd =  open(out.c_str(),O_WRONLY|O_CREAT|O_TRUNC, 0644);
            if(fd<0){
                perror("open sys call");
                exit(0);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }
        if(execvp(args[0],args.data())<0){
            perror("evecvp error");
            exit(1);
        }
    }
    else if(pid>0){
        fg_pid = pid;
        if(!bg){
            waitpid(pid, NULL, WUNTRACED); 
        }
        else cout<<"PID: "<<pid<<endl;
        fg_pid = 0;
    }
    else{
        perror("fork failed");
        return;
    }
}  

void pipeline(char* cmd){
    vector<string>args;
    char* rem = strtok(cmd,"|");
    while(rem){
        args.push_back(rem);
        rem = strtok(NULL,"|");
    }
    int n = args.size();
    if(n==0)return;
    char* c = strdup(args[0].c_str());
    char* temp = strdup(args[0].c_str());
    char* first = strtok(temp," \t");
    if(strcmp(first,"cd")==0 and n==1){
        free(temp);
        run(c);
        return;
    }
    free(temp);
    for(string s:args){
        char* c = strdup(s.c_str());
        char* temp = strdup(args[0].c_str());
        char* first = strtok(temp," \t");
        if(strcmp(first,"cd")==0){
            cout<<"Not supported"<<endl;
            free(temp);
            return;
        }
        free(temp);
    }
    int pipes[2*(n-1)];
    for(int i=0;i<n;i++){
        if(pipe(pipes+2*i)<0){
            perror("pipe error");
            return;
        }
    }
    for(int i=0;i<n;i++){
        string c = args[i];
        char* raw = strdup(c.c_str());
        pid_t pid = fork();
        if (pid == 0) {
            if (i > 0) {
                dup2(pipes[(i-1)*2], STDIN_FILENO);
            }
            if (i < n-1) {
                dup2(pipes[i*2+1], STDOUT_FILENO);
            }
            for (int j=0; j<2*(n-1); j++) close(pipes[j]);
            run(raw);
            exit(0);
        }
        free(raw);
    }
    for(int j=0;j<2*(n-1);j++){
        close(pipes[j]);
    }
    for (int i=0; i<n; i++){
        wait(NULL);
    }
}
