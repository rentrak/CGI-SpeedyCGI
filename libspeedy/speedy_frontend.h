
char *speedy_getclient(SpeedyQueue *q, int *s, int *e);
char *speedy_comm_init(unsigned short port, int *s, int *e);
int speedy_sendenv_size(char *envp[], char *scr_argv[]);
void speedy_sendenv_fill(
    char *buf, char *envp[], char *scr_argv[], int secret_word
);
char *speedy_exec_client(char **argv, int lstn);
char *speedy_do_listen(SpeedyQueue *q, PersistInfo *pinfo, int *lstn);
