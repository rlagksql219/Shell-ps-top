#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <ctype.h>
#include <sys/wait.h>
#include <pwd.h>

int IsDigit(char* filename);
char** read_stat(char* proc_file);
char* seek_stat(char** proc_stat);
int uptime();
double use_cpu(unsigned long utime, unsigned long stime, unsigned long starttime, int seconds); //프로세스의 cpu 사용률
double use_memory(long rss);
char* seek_tty(void);

void print_ps(char** proc_stat);
void print_a(char** proc_stat, struct passwd* username);
void print_u(char** proc_stat, struct passwd* username);
void print_x(char** proc_stat, struct passwd* username);
void print_au(char** proc_stat, struct passwd* username);
void print_ux(char** proc_stat, struct passwd* username);


int main(int argc, char* argv[])
{
	char proc_file[64];
	char** proc_stat;
	DIR* directory;
	struct dirent* entry = NULL;
	struct stat lstat;
 
	if (argc == 1)
		printf(" PID		TTY		TIME			CMD\n");
	else {
		char* ptr = strchr(argv[1], 'u');

		if (ptr == NULL)
			printf("	PID        TTY     STAT        TIME            CMD\n");
		else
			printf("	USER	PID	%%CPU	%%MEM	VSZ		RSS	TTY	STAT	START	TIME	CMD\n");
	}


	if ((directory = opendir("/proc")) == NULL) { //proc 디렉토리 열어서 정보 가져옴
		perror("opendir error :");
		exit(0);
	}

	while ((entry = readdir(directory)) != NULL) {
		if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
			sprintf(proc_file, "/proc/%s/stat", entry->d_name);

			if (access(proc_file, F_OK) != 0) { //stat 파일 존재하는지 확인
				continue;
			}

			if (IsDigit(entry->d_name)) { //디렉토리 이름이 숫자인 경우

				struct passwd* username;

				stat(proc_file, &lstat);
				proc_stat = read_stat(proc_file); //stat 파일 읽음

				username = getpwuid(lstat.st_uid);
			

				if (argc == 1) 
					print_ps(proc_stat);
				else {
					if (strcmp(argv[1], "a") == 0) //a 옵션
						print_a(proc_stat, username);
					else if (strcmp(argv[1], "u") == 0) //u옵션
						print_u(proc_stat, username);
					else if (strcmp(argv[1], "x") == 0 || strcmp(argv[1], "ax") == 0 || strcmp(argv[1], "xa") == 0) //x옵션
						print_x(proc_stat, username);
					else if (strcmp(argv[1], "au") == 0 || strcmp(argv[1], "ua") == 0)
						print_au(proc_stat, username);
					else
						print_ux(proc_stat, username);
				}


				
				// Freeing the allocated memory	
				for (int i = 0; proc_stat[i] != NULL; i++) {
					free(proc_stat[i]);
				}

				free(proc_stat);
			}
		}
	}

	return 0;
}


int IsDigit(char* filename) //파일 이름이 숫자인지 확인
{
	int i;

	for (i = 0; i < strlen(filename); i++) {
		if (isdigit(filename[i]) == 0)
			return 0;
	}

	return 1;
}


char** read_stat(char* proc_file)
{
	FILE* fp;
	char line[1024];
	char** proc_stat = (char**)malloc(64 * sizeof(char*));
	char* proc_stat_token = (char*)malloc(64 * sizeof(char));
	int i, tokenIndex = 0, tokenNo = 0;

	bzero(line, sizeof(line));
	
	fp = fopen(proc_file, "r");
	
	if (fp == NULL) {
		perror("error : ");
		exit(0);
	}
	fgets(line, sizeof(line), fp);

	for (i = 0; i < strlen(line); i++) {
		char readChar = line[i];
		int mark = 0;

		if (readChar == ' ' || readChar == '\n' || readChar == '\t') {
			proc_stat_token[tokenIndex] = '\0';

			if (tokenIndex != 0) {
				proc_stat[tokenNo] = (char*)malloc(64 * sizeof(char));
				if (tokenNo == 1) {
					if (strchr(proc_stat_token, ')') != NULL) {
						strcpy(proc_stat[tokenNo++], proc_stat_token);
						tokenIndex = 0;
					}
				}
				else {
					strcpy(proc_stat[tokenNo++], proc_stat_token);
					tokenIndex = 0;
				}
			}
		}

		else
			proc_stat_token[tokenIndex++] = readChar;
	}

	free(proc_stat_token);
	proc_stat[tokenNo] = NULL;

	return proc_stat;
}


char* seek_stat(char** proc_stat)
{
	char extra_stat[64] = { 0,};

	if (atoi(proc_stat[17]) >= -11 && atoi(proc_stat[17]) <= -2) //높은 우선순위
		extra_stat[0] = '<';
	if (atoi(proc_stat[17]) >= 1 && atoi(proc_stat[17]) <= 99) //낮은 우선순위
		extra_stat[strlen(extra_stat)] = 'N';

	FILE* fp;
	char filename[1024] = { 0, };
	char buf[1024] = { 0, };
	
	char* real_stat = malloc(strlen(buf) + 1);


	sprintf(filename, "/proc/%s/status", proc_stat[0]);
	fp = fopen(filename, "r");

	if (fp == NULL) {
		perror("erro : ");
		exit(0);
	}

	for (int i = 0; i < 19; i++) {
		fgets(buf, 1024, fp);
		
		if (i != 18)
			memset(buf, 0, sizeof(buf));
	}

	fclose(fp);


	if (strstr(buf, " 0 KB") == NULL) //lock 된 page 보유
		extra_stat[strlen(extra_stat)] = 'L';
	if (atoi(proc_stat[0]) == atoi(proc_stat[4])) //세션 리더
		extra_stat[strlen(extra_stat)] = 's';
	if (atoi(proc_stat[19]) >= 2) //멀티스레드
		extra_stat[strlen(extra_stat)] = 'l';
	if (atoi(proc_stat[7]) != -1) //foreground process group
		extra_stat[strlen(extra_stat)] = '+';

	sprintf(real_stat, "%s%s", proc_stat[2], extra_stat);

	return real_stat;
}


int uptime() //os가 부팅 후에 경과한 시간
{
	FILE* fp;
	char buf[36];
	double stime;
	double idletime;

	if ((fp = fopen("/proc/uptime", "r")) == NULL) {
		perror("fopen error : ");
		exit(0);
	}

	fgets(buf, 36, fp);
	sscanf(buf, "%lf %lf", &stime, &idletime);

	fclose(fp);

	return (int)stime;
}


double use_cpu(unsigned long utime, unsigned long stime, unsigned long starttime, int seconds) //프로세스의 cpu 사용률
{
	unsigned long long total_time;
	double using_cpu = 0;

	total_time = utime + stime; //user jiffies + kernal jiffies
								//user jiffies: user mode에서 프로세스가 스케쥴링되면서 사용한 jiffies 수
								//kernal jiffies: kernal mode에서 프로세스가 스케쥴링되면서 사용한 jiffies 수
	seconds = seconds - (int)(starttime / 100.); //starttime: os가 시작된 후, 몇 jiffies가 지나고 프로세스가 시작되었는지

	if (seconds)
		using_cpu = (double)((total_time * 1000ULL / 100.) / seconds);

	return using_cpu;
}


double use_memory(long rss)
{
	FILE* fp;
	char tmp[1024] = { 0, };
	char memtotal[1024] = { 0, };
	char memfree[1024] = { 0, };
	char buffer[1024] = { 0, };
	char cache[1024] = { 0, };
	long used_memory = 0;
	long free_memory = 0;
	double using_memory = 0;
	char* ptr1;
	char* ptr2;
	char* ptr3;
	char* ptr4;

	fp = fopen("/proc/meminfo", "r");

	if (fp == NULL) {
		perror("erro : ");
		exit(0);
	}

	fgets(memtotal, 1024, fp);
	ptr1 = strtok(memtotal, " ");
	ptr1 = strtok(NULL, " ");

	fgets(memfree, 1024, fp);
	ptr2 = strtok(memfree, " ");
	ptr2 = strtok(NULL, " ");

	fgets(tmp, 1024, fp);

	fgets(buffer, 1024, fp);
	ptr3 = strtok(buffer, " ");
	ptr3 = strtok(NULL, " ");

	fgets(cache, 1024, fp);
	ptr4 = strtok(cache, " ");
	ptr4 = strtok(NULL, " ");

	fclose(fp);
	
	used_memory = atol(ptr1) - atol(ptr2) - atol(ptr3) - atol(ptr4);
	free_memory = atol(ptr1) + atol(ptr3) + atol(ptr4);

	double type_tmp = (double)(rss/ (used_memory + free_memory));
	using_memory = (type_tmp* 100);

	return using_memory;
}


char* seek_tty(void) 
{
	/* 표준입력장치에 연결되어 있는 터미널의 파일명 출력 */
	FILE* fp;
	char tty_token_buf[1024] = { 0, };
	char* tty_token_ptr;
	int need_token_split_mark = 0;

	fp = popen("tty", "r");
	if (fp == NULL) {
		perror("erro : ");
		exit(0);
	}

	fgets(tty_token_buf, 1024, fp);
	*(tty_token_buf+(strlen(tty_token_buf)-1))=0; //개행 문자 제거

	if (strstr(tty_token_buf, "tty") == NULL) //pts
		need_token_split_mark = 1;
	
	if (strstr(tty_token_buf, "?")) {
		tty_token_ptr = tty_token_buf;

		return tty_token_ptr;
	}

	tty_token_ptr = strtok(tty_token_buf, "/");
	tty_token_ptr = strtok(NULL, "/");
	
	if (need_token_split_mark == 1) { //pts
		tty_token_ptr = strtok(NULL, "/");
		//tty_token_ptr = strtok(NULL, "/");
	}
	else //tty
		;//tty_token_ptr = strtok(NULL, "/");

	return tty_token_ptr;
}


/********************print*************************/

void print_ps(char** proc_stat)
{
	/* Debug code

	printf("==========process info==========\n");
	for (int i=0; proc_stat[i]!=NULL; i++) {
		printf("[%d] %s\n", i+1, proc_stat[i]);
	}
	printf("\n");

	*/

	long total_cpu_time;
	int h, m, s;
	char cmd[10000] = { 0, };
	char* tty;
	int tty_mark = 0;
	char pts_tty[1024] = { 0, };

	tty = seek_tty();

	if (strstr(tty, "tty") == NULL && strstr(tty, "?") == NULL) { //pts
		tty_mark = 1;
		sprintf(pts_tty, "pts/%s", tty);
	}

	total_cpu_time = (long)((atoi(proc_stat[13]) + atoi(proc_stat[14])) / 100);
	h = total_cpu_time / 3600;
	m = (total_cpu_time - h * 3600) / 60;
	s = (total_cpu_time - (h * 3600) - (m * 60));

	for (int i = 0; i < strlen(proc_stat[1]) - 1; i++) {
		cmd[i] = proc_stat[1][i + 1];
	}
	cmd[strlen(proc_stat[1]) - 2] = '\0';


	if (strcmp(proc_stat[2], "R") == 0) { //현재 실행중인 프로세스만 출력
		if (tty_mark == 1)
			printf(" %s		%s		%02d:%02d:%02d		%s\n", proc_stat[0], pts_tty, h, m, s, cmd);
		else
			printf(" %s		%s		%02d:%02d:%02d		%s\n", proc_stat[0], tty, h, m, s, cmd);
	}
}


void print_a(char** proc_stat, struct passwd* username)
{
	long total_cpu_time;
	int h, m, s;
	char cmd[1024] = { 0, };
	char* real_stat = malloc(1024 + 1);

	char* tty;
	int tty_mark = 0;
	char pts_tty[1024] = { 0, };

	tty = seek_tty();

	if (strstr(tty, "tty") == NULL && strstr(tty, "?") == NULL) { //pts
		tty_mark = 1;
		sprintf(pts_tty, "pts/%s", tty);
	}

	real_stat = seek_stat(proc_stat);

	total_cpu_time = (long)((atoi(proc_stat[13]) + atoi(proc_stat[14])) / 100);
	h = total_cpu_time / 3600;
	m = (total_cpu_time - h * 3600) / 60;
	s = (total_cpu_time - (h * 3600) - (m * 60));

	for (int i = 0; i < strlen(proc_stat[1]) - 1; i++) {
		cmd[i] = proc_stat[1][i + 1];
	}
	cmd[strlen(proc_stat[1]) - 2] = '\0';


	if (strcmp(proc_stat[2], "R") == 0 || strcmp(proc_stat[2], "S") == 0) { //현재 실행중인 프로세스만 출력
		if (tty_mark == 1)
			printf("	%5s	%s	%s	%02d:%02d:%02d	%s\n", proc_stat[0], pts_tty, real_stat, h, m, s, cmd);
		else
			printf("	%5s	%s	%s	%02d:%02d:%02d	%s\n", proc_stat[0], tty, real_stat, h, m, s, cmd);
	}

	free(real_stat);
}


void print_u(char** proc_stat, struct passwd* username)
{
	FILE* fp;
	long total_cpu_time;
	int h, m, s;
	int h2, m2, s2;
	char cmd[1024] = { 0, };
	char* real_stat = malloc(1024 + 1);
	int seconds = 0;
	double using_cpu;
	double using_memory;
	
	using_memory = use_memory(use_memory(atol(proc_stat[23])));
	seconds = uptime();
	using_cpu = use_cpu(atol(proc_stat[13]), atol(proc_stat[14]), atol(proc_stat[21]), seconds);

	FILE* fp2;
	char filename[1024] = { 0, };
	char buf2[1024] = { 0, };
	char* rss_ptr;

	sprintf(filename, "/proc/%s/status", proc_stat[0]);
	fp2 = fopen(filename, "r");

	if (fp2 == NULL) {
		perror("erro : ");
		exit(0);
	}

	for (int i = 0; i < 18; i++) {
		fgets(buf2, 1024, fp2);

		if (i != 17)
			memset(buf2, 0, sizeof(buf2));
	}

	rss_ptr = strtok(buf2, " ");
	rss_ptr = strtok(NULL, " ");
	if (rss_ptr == '\0') {
		rss_ptr = malloc(20);
		sprintf(rss_ptr, "0");
	}

	fclose(fp2);


	h2 = atoi(proc_stat[21]) / 3600;
	m2 = (atoi(proc_stat[21]) - h2 * 3600) / 60;
	s2 = (atoi(proc_stat[21]) - (h2 * 3600) - (m2 * 60));

	char* tty;
	int tty_mark = 0;
	char pts_tty[1024] = { 0, };

	tty = seek_tty();

	if (strstr(tty, "tty") == NULL && strstr(tty, "?") == NULL) { //pts
		tty_mark = 1;
		sprintf(pts_tty, "pts/%s", tty);
	}

	real_stat = seek_stat(proc_stat);

	total_cpu_time = (long)((atoi(proc_stat[13]) + atoi(proc_stat[14])) / 100);
	h = total_cpu_time / 3600;
	m = (total_cpu_time - h * 3600) / 60;
	s = (total_cpu_time - (h * 3600) - (m * 60));

	for (int i = 0; i < strlen(proc_stat[1]) - 1; i++) {
		cmd[i] = proc_stat[1][i + 1];
	}
	cmd[strlen(proc_stat[1]) - 2] = '\0';


	if (strcmp(proc_stat[2], "R") == 0) { //현재 실행중인 프로세스만 출력
		if (tty_mark == 1)
			printf("%15s	%5s	%0.1f	%0.1f	%s		%s	%s	%s	%02d:%02d	%d:%02d	%s\n", username->pw_name, proc_stat[0], using_cpu, using_memory, rss_ptr, proc_stat[23], pts_tty, real_stat, m2, s2, m, s, cmd);
		else
			printf("%15s	%5s	%0.1f	%0.1f	%s		%s	%s	%s	%02d:%02d	%d:%02d	%s\n", username->pw_name, proc_stat[0], using_cpu, using_memory, rss_ptr, proc_stat[23], tty, real_stat, m2, s2, m, s, cmd);
	}

	free(real_stat);
}


void print_x(char** proc_stat, struct passwd* username)
{
	long total_cpu_time;
	int h, m, s;
	char cmd[1024] = { 0, };
	char* real_stat = malloc(1024 + 1);

	char* tty;
	int tty_mark = 0;
	char pts_tty[1024] = { 0, };

	tty = seek_tty();

	if (strstr(tty, "tty") == NULL && strstr(tty, "?") == NULL) { //pts
		tty_mark = 1;
		sprintf(pts_tty, "pts/%s", tty);
	}

	real_stat = seek_stat(proc_stat);

	total_cpu_time = (long)((atoi(proc_stat[13]) + atoi(proc_stat[14])) / 100);
	h = total_cpu_time / 3600;
	m = (total_cpu_time - h * 3600) / 60;
	s = (total_cpu_time - (h * 3600) - (m * 60));

	for (int i = 0; i < strlen(proc_stat[1]) - 1; i++) {
		cmd[i] = proc_stat[1][i + 1];
	}
	cmd[strlen(proc_stat[1]) - 2] = '\0';



	if (tty_mark == 1)
		printf("	%5s	%s	%s		%02d:%02d:%02d	%s\n", proc_stat[0], pts_tty, real_stat, h, m, s, cmd);
	else
		printf("	%5s	%s	%s		%02d:%02d:%02d	%s\n", proc_stat[0], tty, real_stat, h, m, s, cmd);

	free(real_stat);
}


void print_au(char** proc_stat, struct passwd* username)
{
	FILE* fp;
	long total_cpu_time;
	int h, m, s;
	int h2, m2, s2;
	char cmd[1024] = { 0, };
	char* real_stat = malloc(1024 + 1);
	int seconds = 0;
	double using_cpu;
	double using_memory;

	using_memory = use_memory(use_memory(atol(proc_stat[23])));
	seconds = uptime();
	using_cpu = use_cpu(atol(proc_stat[13]), atol(proc_stat[14]), atol(proc_stat[21]), seconds);

	FILE* fp2;
	char filename[1024] = { 0, };
	char buf2[1024] = { 0, };
	char* rss_ptr;

	sprintf(filename, "/proc/%s/status", proc_stat[0]);
	fp2 = fopen(filename, "r");

	if (fp2 == NULL) {
		perror("erro : ");
		exit(0);
	}

	for (int i = 0; i < 18; i++) {
		fgets(buf2, 1024, fp2);

		if (i != 17)
			memset(buf2, 0, sizeof(buf2));
	}

	rss_ptr = strtok(buf2, " ");
	rss_ptr = strtok(NULL, " ");
	if (rss_ptr == '\0') {
		rss_ptr = malloc(20);
		sprintf(rss_ptr, "0");
	}

	fclose(fp2);


	h2 = atoi(proc_stat[21]) / 3600;
	m2 = (atoi(proc_stat[21]) - h2 * 3600) / 60;
	s2 = (atoi(proc_stat[21]) - (h2 * 3600) - (m2 * 60));

	char* tty;
	int tty_mark = 0;
	char pts_tty[1024] = { 0, };

	tty = seek_tty();

	if (strstr(tty, "tty") == NULL && strstr(tty, "?") == NULL) { //pts
		tty_mark = 1;
		sprintf(pts_tty, "pts/%s", tty);
	}

	real_stat = seek_stat(proc_stat);

	total_cpu_time = (long)((atoi(proc_stat[13]) + atoi(proc_stat[14])) / 100);
	h = total_cpu_time / 3600;
	m = (total_cpu_time - h * 3600) / 60;
	s = (total_cpu_time - (h * 3600) - (m * 60));

	for (int i = 0; i < strlen(proc_stat[1]) - 1; i++) {
		cmd[i] = proc_stat[1][i + 1];
	}
	cmd[strlen(proc_stat[1]) - 2] = '\0';


	if (strcmp(proc_stat[2], "R") == 0 || strcmp(proc_stat[2], "S") == 0) { //현재 실행중인 프로세스만 출력
		if (tty_mark == 1)
			printf("%10s	%5s	%0.1f	%0.1f	%s		%s	%s	%s	%02d:%02d	%d:%02d	%s\n", username->pw_name, proc_stat[0], using_cpu, using_memory, rss_ptr, proc_stat[23], pts_tty, real_stat, m2, s2, m, s, cmd);
		else
			printf("%10s	%5s	%0.1f	%0.1f	%s		%s	%s	%s	%02d:%02d	%d:%02d	%s\n", username->pw_name, proc_stat[0], using_cpu, using_memory, rss_ptr, proc_stat[23], tty, real_stat, m2, s2, m, s, cmd);
	}

	free(real_stat);
}


void print_ux(char** proc_stat, struct passwd* username)
{
	FILE* fp;
	long total_cpu_time;
	int h, m, s;
	int h2, m2, s2;
	char cmd[1024] = { 0, };
	char* real_stat = malloc(1024 + 1);
	int seconds = 0;
	double using_cpu;
	double using_memory;

	using_memory = use_memory(use_memory(atol(proc_stat[23])));
	seconds = uptime();
	using_cpu = use_cpu(atol(proc_stat[13]), atol(proc_stat[14]), atol(proc_stat[21]), seconds);

	FILE* fp2;
	char filename[1024] = { 0, };
	char buf2[1024] = { 0, };
	char* rss_ptr;

	sprintf(filename, "/proc/%s/status", proc_stat[0]);
	fp2 = fopen(filename, "r");

	if (fp2 == NULL) {
		perror("erro : ");
		exit(0);
	}

	for (int i = 0; i < 18; i++) {
		fgets(buf2, 1024, fp2);

		if (i != 17)
			memset(buf2, 0, sizeof(buf2));
	}

	rss_ptr = strtok(buf2, " ");
	rss_ptr = strtok(NULL, " ");
	if (rss_ptr == '\0') {
		rss_ptr = malloc(20);
		sprintf(rss_ptr, "0");
	}

	fclose(fp2);


	h2 = atoi(proc_stat[21]) / 3600;
	m2 = (atoi(proc_stat[21]) - h2 * 3600) / 60;
	s2 = (atoi(proc_stat[21]) - (h2 * 3600) - (m2 * 60));

	char* tty;
	int tty_mark = 0;
	char pts_tty[1024] = { 0, };

	tty = seek_tty();

	if (strstr(tty, "tty") == NULL && strstr(tty, "?") == NULL) { //pts
		tty_mark = 1;
		sprintf(pts_tty, "pts/%s", tty);
	}

	real_stat = seek_stat(proc_stat);

	total_cpu_time = (long)((atoi(proc_stat[13]) + atoi(proc_stat[14])) / 100);
	h = total_cpu_time / 3600;
	m = (total_cpu_time - h * 3600) / 60;
	s = (total_cpu_time - (h * 3600) - (m * 60));

	for (int i = 0; i < strlen(proc_stat[1]) - 1; i++) {
		cmd[i] = proc_stat[1][i + 1];
	}
	cmd[strlen(proc_stat[1]) - 2] = '\0';

	if (tty_mark == 1)
		printf("%15s	%5s	%0.1f	%0.1f	%s	%10s	%s	%s	%02d:%02d	%d:%02d	%s\n", username->pw_name, proc_stat[0], using_cpu, using_memory, rss_ptr, proc_stat[23], pts_tty, real_stat, m2, s2, m, s, cmd);
	else
		printf("%15s	%5s	%0.1f	%0.1f	%s	%10s	%s	%s	%02d:%02d	%d:%02d	%s\n", username->pw_name, proc_stat[0], using_cpu, using_memory, rss_ptr, proc_stat[23], tty, real_stat, m2, s2, m, s, cmd);

	free(real_stat);
}
