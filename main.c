#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <unistd.h>

static long n = 0;
static long mutex = 0;
static long eattime = 0;
static long thinktime = 0;

enum STATE
{
    STATE_THINKING,
    STATE_HUNGRY,
    STATE_EATING,
};

enum VISUAL
{
    MS = 1000,
};

static int semid = 0;
static int shmid = 0;
static int *state = NULL;

static long
mmod(long a, long b)
{
    long m = a % b;
    if (m < 0) {
        m = (b < 0) ? m - b : m + b;
    }

    return m;
}

static long
left(long i)
{
    return mmod(i - 1, n);
}

static long
right(long i)
{
    return mmod(i + 1, n);
}

static void
think(void)
{
    sleep(thinktime);
}

static void
eat(void)
{
    sleep(eattime);
}

static void
up(long no)
{
    struct sembuf ops[] = { {
            .sem_num = no,
            .sem_op = 1,
            .sem_flg = 0,
    } };
    semop(semid, ops, 1);
}

static void
down(long no)
{
    struct sembuf ops[] = { {
            .sem_num = no,
            .sem_op = -1,
            .sem_flg = 0,
    } };
    semop(semid, ops, 1);
}

static void
test(long no)
{
    if (state[no] == STATE_HUNGRY && state[left(no)] != STATE_EATING &&
        state[right(no)] != STATE_EATING) {
        state[no] = STATE_EATING;
        up(no);
    }
}

static void
take_forks(long no)
{
    down(mutex);
    state[no] = STATE_HUNGRY;
    test(no);
    up(mutex);

    down(no);
}

static void
put_forks(long no)
{
    down(mutex);
    state[no] = STATE_THINKING;
    test(left(no));
    test(right(no));
    up(mutex);
}

static void
philosopher(long no)
{
    for (;;) {
        think();
        take_forks(no);
        eat();
        put_forks(no);
    }
}

static void
stop(int signo)
{
    signal(signo, SIG_IGN);

    killpg(0, signo);

    semctl(semid, 0, IPC_RMID, 0);
    shmdt(state);
    shmctl(shmid, IPC_RMID, 0);

    while (wait(NULL) != -1)
        ;

    _exit(EXIT_SUCCESS);
}

static void
redraw(void)
{
    puts("\033[6;0H"); // Set 6,0 coords

    for (long i = 0; i < n; ++i) {
        char c = '-';
        switch (state[i]) {
        case STATE_THINKING:
            c = 'T';
            break;
        case STATE_HUNGRY:
            c = 'H';
            break;
        case STATE_EATING:
            c = 'E';
            break;
        default:
            break;
        }
        putchar(c);
        if (i != n - 1) {
            putchar('|');
        }
    }

    fflush(stdout);
}

static void
visual(void)
{
    puts("\033[2J"); // Clear screen
    puts("\033[0;0H" // Set 0,0 coords
         "Dining philosophers problem\n"
         "(Press ^C to stop program)\n"
         "T - Thinking\n"
         "H - Hungry\n"
         "E - Eating\n");

    for (;;) {
        redraw();
        usleep(20 * MS);
    }
}

int
main(int argc, char *argv[])
{
    if (argc < 4) {
        fprintf(stderr, "Usage: %s nphilosophers thinktime eattime\n", argv[0]);
        _exit(EXIT_FAILURE);
    }

    mutex = n = strtol(argv[1], NULL, 10);
    thinktime = strtol(argv[2], NULL, 10);
    eattime = strtol(argv[3], NULL, 10);

    key_t key = ftok(argv[0], 1);

    semid = semget(key, n, IPC_CREAT | 0600);
    if (semid == -1) {
        perror("semget");
        _exit(EXIT_FAILURE);
    }
    shmid = shmget(key, n * sizeof(int), IPC_CREAT | 0600);
    if (shmid == -1) {
        perror("shmget");
        _exit(EXIT_FAILURE);
    }

    state = shmat(shmid, NULL, 0);

    signal(SIGINT, stop);

    pid_t pid = fork();
    if (pid == -1) {
        stop(SIGINT);
    } else if (pid == 0) {
        signal(SIGINT, SIG_DFL);
        visual();
    }

    for (long no = 0; no < n; ++no) {
        pid = fork();
        if (pid == -1) {
            stop(SIGINT);
        } else if (pid == 0) {
            signal(SIGINT, SIG_DFL);
            philosopher(no);
        }
        usleep((MS / n) * MS);
    }

    pause();
}

