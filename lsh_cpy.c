#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int lsh_cd(char **args);
int lsh_help(char **args);
int lsh_exit(char **args);

char *builtin_str[] = {
    "cd",
    "help",
    "exit"
};

int lsh_num_builtins() {
    // builtin_str に何個のコマンドがあるか調べる
    return sizeof(builtin_str) / sizeof(char *);
}

// 関数ポインタ？
int (*builtin_func[])(char **) = {
    &lsh_cd,
    &lsh_help,
    &lsh_exit
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