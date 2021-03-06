#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>

int lsh_cd(char **args);
int lsh_ls(char **args);
int lsh_cat(char **args);
int lsh_help(char **args);
int lsh_exit(char **args);


char *builtin_str[] = {
    "cd",
    "ls",
    "cat",
    "help",
    "exit",
    "cp",
};

int lsh_num_builtins() {
    // builtin_str に何個のコマンドがあるか調べる
    return sizeof(builtin_str) / sizeof(char *);
}

// 関数ポインタ？
int (*builtin_func[])(char **) = {
    &lsh_cd,
    &lsh_ls,
    &lsh_cat,
    &lsh_help,
    &lsh_exit,
    &lsh_cp,
};

// args[0] == "cd", args[1] == directory
int lsh_cd(char **args)
{
    if (args[1] == NULL) {
        fprintf(stderr, "lsh: expected argument to \"cd\"\n");
    } else {
        // chdirディレクトリ移動
        // 成功したら0、失敗したら-1
        if (chdir(args[1]) != 0) {
            perror("lsh");
        }
    }
    return 1;
}

int lsh_ls(char **args)
{
    char dirname[256];
    DIR *dir;
    struct dirent *ent;

    if (args[1] == NULL) {
        strcpy (dirname, ".");
    } else {
        strcpy(dirname, args[1]);
    }

    // ディレクトリをオープンにしてreaddir()とかで読み取れるようにする
    // 正常に終了したら、dirオブジェクトへのポインタを戻す
    // NULLポインタを返す
    dir = opendir(dirname);
    if (dir == NULL) {
        fprintf(stderr, "unable to opendir %s\n", dirname);
    }

    while ((ent = readdir(dir)) != NULL) {
        // 隠しファイルスキップ
        if (ent->d_name[0] == '.') {
            continue;
        }
        // ent->d_reclen: レコード長(バイトの単位)
        // ent->d_namlen: ディレクトリ名の文字列の長さ
        // ent->d_name:   ディレクトリ名
        printf("%s ", ent->d_name);
    }
    printf("\n");
    closedir(dir);

    return 1;
}

#define BUFSIZE 4096
int lsh_cat(char **args)
{
    int i;

    if (args[1] == NULL) {
        fprintf(stderr, "file name not given\n");
    }

    i = 1;
    while (args[i] != NULL)    
    {
        int fd;
        unsigned char buf[BUFSIZE];
        ssize_t cc;

        // O_RDONLY: 読み込み可
        // 失敗：-1
        // 成功：ファイルデスクリプタ
        if ((fd = open(args[i], O_RDONLY)) == -1)
        {
            fprintf(stderr, "no such file\n");
            break;
        }
        for (;;) {
            // read:
            // fd: 読み出すデータが格納されたファイルディスクリプタ
            // buf: 読み出したデータを格納するバッファの先頭アドレス
            // count: 読み込む最大バイト数
            // 戻り値: ssize_t型に返す
            // 0: ファイルのオフセットがすでにEOF
            // -1: エラーが発生
            cc = read(fd, buf, sizeof buf);
            if (cc == -1)
            {
                fprintf(stderr, "read error\n");
                break;
            }
            if (cc == 0)
                break;
            if (write(STDOUT_FILENO, buf, cc) == -1)
            {
                fprintf(stderr, "write error\n");
                break;
            }
        }
        if (close(fd) == -1)
        {
            fprintf(stderr, "close error\n");
            break;
        }
        i++;
    }
    printf("\n");
    return 1;
}

int lsh_help(char **args)
{
    int i;
    printf("Type program names and arguments, and hit enter.\n");
    printf("The following are built in:\n");


    for (i = 0; i < lsh_num_builtins(); i++) {
        printf(" %s\n", builtin_str[i]);
    }
    printf("Use the man command for information on other programs.\n");
    return 1;
}

int lsh_exit(char **args)
{
    return 0;
}

int lsh_cp(char **args)
{
    FILE *file, *new_file;
    int i;
    i = 1;
    while (args[i] != NULL)
        i++;
    if (i < 3) {
        fprintf(stderr, "file num is too small")
        return 0;
    } else if (i > 3) {
        fprintf(stderr, "file num is too large")
        return 0;
    }

    file = fopen(argv[1], "rb");
    if (!file) {
        fprintf(stderr, "can't open file\n");
        return 0;
    }
    new_file = fopen(argv[2], "wb");
    if (!new_file) {
        fprintf(stderr, "can't open file\n");
        return 0;
    }

    char buf[4096];
    size_t size;
    while ((size=fread(buf, sizeof(char), sizeof(buf), file)) > 0) {
        if (fwrite(buf, sizeof(char), size, new_file) <= 0) {
            fprintf(stderr, "can't copy")
            return 0;
        }
    }

    fclose(file);
    fclose(new_file);
    return 1;
}


int lsh_launch(char **args)
{
    // pid_t -> unistd内で定義　intと似ている
    // pidを表す変数ということを言いたいはず
    pid_t pid;
    int status;

    // fork() -> 新しいプロセスを作成するシステムコール
    // シェルでコマンドを実行するときに、forkシステムコールをして、
    // シェルの子プロセスとして、コマンドを実行する
    // シェルでパイプを利用して、複数のコマンドを指定したときには、コマンドの数だけ
    // forkを行い、それぞれのプロセスの入出力をdup/pipeのシステムコールを利用して、
    // つなぎ合わせて、コマンドをexecシステムコールで実行する

    // fork：子プロセスには0を返す、システムコールが失敗したら親プロセスに-1が返され
    //       子プロセスは作成されない
    pid = fork();
    if (pid == 0) {
        // execvp(const char *file, char *const argv[])
        // file: 実行するファイルのパス
        // Argv: パラメータへのポインタの配列
        // 成功したら呼び出しプロセスに戻ることはない。
        // 失敗したら-1が返る
        // やっぱりよくわからん
        if (execvp(args[0], args) == -1) {
            // perror("lsh") -> lsh: No such file or directoryて出力される
            perror("lsh");
        }
        exit(EXIT_FAILURE);
    } else if (pid < 0) {
        // エラー
        perror("lsh");
    } else {
        // 親プロセスは子プロセスが終了か停止するまでプロセス中断
        // &status: リターンされる状態
        // WUNTRACED: オプション
        do {
            waitpid(pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
        // WIFEXITED(status): 子プロセスが正常に終了したら真を返す
        // WIFSIGNALED(status): 子プロセスの終了ステータスを返す。 
        }
    return 1;
}

int lsh_excute(char **args)
{
    int i;

    if (args[0] == NULL) {
        // コマンドが空の場合
        return 1;
    }

    for (i = 0; i < lsh_num_builtins(); i++) {
        // args builtin_strが同じ文字列なら
        if (strcmp(args[0], builtin_str[i]) == 0) {
            return (*builtin_func[i])(args); // <- この記述方法何？
                                             // 多分関数ポインタな気がする
        }
    }

    // 自分で実装していないときにパソコンにあるファイルを見に行く
    return lsh_launch(args);
}

char *lsh_read_line(void)
{
// ****************************************************
// ここの処理がよくわからん
// ****************************************************
#ifdef LSH_USE_STD_GETLINE
    char *line = NULL;
    ssize_t bufsize = 0; 
    if (getline(&line, &bufsize, stdin) == -1) {
        if (feof(stdin)) {
            exit(EXIT_SUCCESS);
        } else {
            perror("lsh: getline\n");
            exit(EXIT_FAILURE)
        }
    }
    return line;

#else
#define LSH_RL_BUFSIZE 1024
    int bufsize = LSH_RL_BUFSIZE;
    int position = 0;
    char *buffer = malloc(sizeof(char) * bufsize);
    int c;

    if (!buffer) {
        // malloc失敗時のエラー
        fprintf(stderr, "lsh: allocation error\n");
        exit(EXIT_FAILURE);
    }

    while (1) {
        // asciiとして文字列を取得
        c = getchar();

        if (c == EOF) {
            exit(EXIT_SUCCESS);
        } else if (c == '\n') {
            // コマンドラインをエンターで実行した位置
            buffer[position] = '\0';
            return buffer;
        } else {
            // 文字列が書かれている時はbufferに格納
            buffer[position] = c;
        }
        position++;

        // bufferをオーバーした時
        if (position >= bufsize) {
            bufsize += LSH_RL_BUFSIZE;
            // bufferをサイズ変更してメモリを再割り当てを行う
            buffer = realloc(buffer, bufsize);
            if (!buffer) {
                fprintf(stderr, "lsh: allocation error\n");
                exit(EXIT_FAILURE);
            }
        }
    }
    #endif
}

#define LSH_TOK_BUFSIZE 64
#define LSH_TOK_DELIM " \t\r\n\a"
char **lsh_split_line(char *line)
{
    int bufsize = LSH_TOK_BUFSIZE, position = 0;
    char **tokens = malloc(bufsize * sizeof(char*));
    char *token, **tokens_backup; // tokens_backupの使い所はどこだ？
                                  // 使わない気がする
    if (!tokens) {
        fprintf(stderr, "lsh: allocation error\n");
        exit(EXIT_FAILURE);
    }

    // strtok -> line を LSH_TOK_DELIMでトークンに分解する
    // 返り値はポインタ
    token = strtok(line, LSH_TOK_DELIM);
    while (token != NULL) {
        tokens[position] = token;
        position++;

        if (position >= bufsize) {
            bufsize += LSH_TOK_BUFSIZE;
            tokens_backup = tokens;
            tokens = realloc(tokens, bufsize * sizeof(char*));
            if (!tokens) {
                free(tokens_backup);
                fprintf(stderr, "lsh: allocation error\n");
                exit(EXIT_FAILURE);
            }
        }
        token = strtok(NULL, LSH_TOK_DELIM);
    }
    tokens[position] = NULL;
    return tokens;
}

void lsh_loop(void)
{
    char *line;
    char **args;
    int status;

    do {
        printf(">");
        line = lsh_read_line();
        args = lsh_split_line(line);
        status = lsh_excute(args);

        free(line);
        free(args);
    } while (status);
}

int main(int argc, char **argv)
{
    lsh_loop();

    // EXIT_SUCCESS -> プログラムが正常終了したことを表す終了コード
    return EXIT_SUCCESS;
}