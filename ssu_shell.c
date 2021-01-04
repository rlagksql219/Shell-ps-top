#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

#define MAX_INPUT_SIZE 1024
#define MAX_TOKEN_SIZE 64
#define MAX_NUM_TOKENS 64

char** tokenize_space(char* line);
char** tokenize_pipe(char* line);
void exec_cmd(char** tokens);
void loop_pipe(char** tokens);


int main(int argc, char* argv[]) {
	char line[MAX_INPUT_SIZE];
	char** tokens;
	int i;

	FILE* fp;
	if (argc == 2) {
		fp = fopen(argv[1], "r");
		if (fp == NULL) {
			printf("File doesn't exists.\n");
			return -1;
		}
	}


	while (1) {
		int check_pipe = 0;

		/* BEGIN: TAKING INPUT */
		bzero(line, sizeof(line));

		if (argc == 2) { // batch mode
			if (fgets(line, sizeof(line), fp) == NULL) { // file reading finished	
				fclose(fp);
				break;
			}

			line[strlen(line) - 1] = '\0';
		}

		else { // interactive mode
			printf("$ ");
			scanf("%[^\n]", line); //개행문자까지 입력받기(띄어쓰기 입력받을 수 o)

			getchar(); //입력 버퍼에 남아있는 개행문자 비우기

			if (strlen(line) == 0) //엔터만 입력한 경우
				continue;
		}

		//printf("Command entered: %s (remove this debug output later)\n", line);
		/* END: TAKING INPUT */

		line[strlen(line)] = '\n'; //terminate with new line
		if (strchr(line, '|') == NULL) {
			tokens = tokenize_space(line);
		}
		else {
			tokens = tokenize_pipe(line);
			check_pipe = 1;
		}

		//do whatever you want with the commands, here we just print them

		if (check_pipe == 0)
			exec_cmd(tokens);

		else
			loop_pipe(tokens);


/*		for(i=0;tokens[i]!=NULL;i++){
			printf("found token %s (remove this debug output later)\n", tokens[i]);
		}
*/


		// Freeing the allocated memory	
		for (i = 0; tokens[i] != NULL; i++) {
			free(tokens[i]);
		}

		free(tokens);
	}

	return 0;
}


/* Splits the string by space and returns the array of tokens
*
*/
char** tokenize_space(char* line)
{

	char** tokens = (char**)malloc(MAX_NUM_TOKENS * sizeof(char*));
	char* token = (char*)malloc(MAX_TOKEN_SIZE * sizeof(char));
	int i, tokenIndex = 0, tokenNo = 0;

	for (i = 0; i < strlen(line); i++) {
		char readChar = line[i];

		if (readChar == ' ' || readChar == '\n' || readChar == '\t') {
			token[tokenIndex] = '\0';

			if (tokenIndex != 0) {
				tokens[tokenNo] = (char*)malloc(MAX_TOKEN_SIZE * sizeof(char));
				strcpy(tokens[tokenNo++], token);
				tokenIndex = 0;
			}
		}
		else {
			token[tokenIndex++] = readChar;
		}
	}

	free(token);
	tokens[tokenNo] = NULL;
	return tokens;
}


char** tokenize_pipe(char* line)
{
	char** tokens = (char**)malloc(MAX_NUM_TOKENS * sizeof(char*));
	char* token = (char*)malloc(MAX_TOKEN_SIZE * sizeof(char));
	int i, tokenIndex = 0, tokenNo = 0;

	for (i = 0; i < strlen(line); i++) {
		char readChar = line[i];

		if (readChar == '|' || readChar == '\n' || readChar == '\t') {
			if (readChar == '|') {
				token[tokenIndex - 1] = '\0';
			}
			else {
				token[tokenIndex] = '\0';
			}

			if (tokenIndex != 0) {
				tokens[tokenNo] = (char*)malloc(MAX_TOKEN_SIZE * sizeof(char));
				strcpy(tokens[tokenNo++], token);
				tokenIndex = 0;
			}
		}
		else {
			int space_check = 0;

			if (readChar == ' ' && tokenIndex == 0) {
				space_check = 1;
			}
			else {
				if (space_check == 1) {
					token[tokenIndex - 1] = readChar;
					tokenIndex++;
				}
				else {
					token[tokenIndex++] = readChar;
				}
			}
		}
	}

	free(token);
	tokens[tokenNo] = NULL;
	return tokens;
}


void exec_cmd(char** tokens)
{
	pid_t pid;
	int status;

	if ((pid = fork()) < 0) { //에러
		fprintf(stderr, "fork error\n");
		exit(1);
	}

	else if (pid == 0) { //자식
		char pwd[1024];
		char pps_path[1024];
		char ttop_path[1024];

		getcwd(pwd, sizeof(pwd));
		sprintf(pps_path, "%s/pps", pwd);
		sprintf(ttop_path, "%s/ttop", pwd);

		if (strcmp(tokens[0], "pps") == 0) { //pps 명령어 처리
			if(execv(pps_path, tokens) == -1) {
				printf("SSUShell : Incorrect command\n");
				exit(1);
			}
		}

		else if (strcmp(tokens[0], "ttop") == 0) { //ttop 명령어 처리
			if(execv(ttop_path, tokens) == -1) {
				printf("SSUShell : Incorrect command\n");
				exit(1);
			}
		}

		else {
			if(execvp(tokens[0], tokens) < 0) {
				printf("SSUShell : Incorrect command\n");
				exit(1);
			}
		}
		exit(EXIT_FAILURE);
	}

	else { //부모
		pid_t waitPid;

		waitPid = wait(&status);

		if (waitPid == -1) {
			printf("에러 넘버 : %d\n", errno);
			perror("wait 함수 오류 반환");
		}

		else {
			if (WIFEXITED(status)) {
				//printf("wait : 자식 프로세스 정상 종료 %d\n", WEXITSTATUS(status));
			}

			else if (WIFSIGNALED(status)) {
				printf("wait : 자식 프로세스 비정상 종료 %d\n", WTERMSIG(status));
			}
		}
	}
}


void loop_pipe(char** tokens)
{
	int fd[2];
	pid_t pid;
	int fd_in = 0;
	int i = 0;
	int status;

	while (tokens[i] != NULL) {
		char line[MAX_INPUT_SIZE] = { 0, };
		char** tmp_tokens;

		strcpy(line, tokens[i]);
		line[strlen(line)] = '\n';
		tmp_tokens = tokenize_space(line);

		/*		for(int j=0; tmp_tokens[j] != NULL; j++) {
					printf("line %s\n", line);
					printf("found tmp_token %s (remove this debug output later)\n", tmp_tokens[j]);
				}
		*/

		pipe(fd);

		if ((pid = fork()) < 0) { //에러
			fprintf(stderr, "fork error\n");
			exit(1);
		}

		else if (pid == 0) { //자식
			dup2(fd_in, 0);
			if (tokens[i + 1] != NULL) {
				dup2(fd[1], 1);
			}
			close(fd[0]);

			char pwd[1024];
			char pps_path[1024];
			char ttop_path[1024];

			getcwd(pwd, sizeof(pwd));
			sprintf(pps_path, "%s/pps", pwd);
			sprintf(ttop_path, "%s/ttop", pwd);

			if (strcmp(tmp_tokens[0], "pps") == 0) { //pps 명령어 처리
				if(execv(pps_path, tmp_tokens) == -1) {
					printf("SSUShell : Incorrect command\n");
					exit(1);
				}
			}

			else if (strcmp(tmp_tokens[0], "ttop") == 0) { //ttop 명령어 처리
				if(execv(ttop_path, tmp_tokens) == -1) {
					printf("SSUShell : Incorrect command\n");
					exit(1);
				}
			}

			else {
				if (execvp(tmp_tokens[0], tmp_tokens) < 0) {
					printf("SSUShell : Incorrect command\n");
					exit(1);
				}
				exit(EXIT_FAILURE);
			}
		}

		else { //부모

			pid_t waitPid;

			waitPid = wait(&status);

			if (waitPid == -1) {
				printf("에러 넘버 : %d\n", errno);
				perror("wait 함수 오류 반환");
			}

			else {
				if (WIFEXITED(status)) {
					//printf("wait : 자식 프로세스 정상 종료 %d\n", WEXITSTATUS(status));
				}

				else if (WIFSIGNALED(status)) {
					printf("wait : 자식 프로세스 비정상 종료 %d\n", WTERMSIG(status));
				}
			}

			close(fd[1]);
			fd_in = fd[0];
			i++;

			for(int j = 0; tmp_tokens[j] != NULL; j++) {
				free(tmp_tokens[j]);
			}

			free(tmp_tokens);


		}
	}
}
