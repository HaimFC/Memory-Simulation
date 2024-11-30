#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/ipc.h>
#include <sys/msg.h>

//------------------------------------------Defines---------------------------------------------------------------------

//==================Probabilities==================

#define WR_RATE 0.5 			// Invoke a memory access prob
#define HIT_RATE 0.5			// request is a hit prob

//==================System properties==================

#define N 5						// Mem slots
#define USED_SLOTS_TH 3 		//

//==================Times==================

#define SIM_TIME 1				// simulation time
#define TIME_BETWEEN_SNAPSHOTS 100000000	// printer prints the “memory” time.
#define MEM_WR_T 1000			// A write to the memory time
#define HD_ACCS_T	1000000		// HD iterations time
#define INTER_MEM_ACCS_T 10000	// Process iterations time
#define NANO 0.000000001        //Use for nano calculation

//==================Threads/Process ID'S==================

#define HD 0
#define MMU 1
#define EVICTER 2
#define PROCESS_1 3
#define PROCESS_2 4

//==================States==================

#define READ 0
#define WRITE 1
#define ACK 2

//------------------------------------------------Structures------------------------------------------------------------

struct buffer {
    long type;
    int content;
    int source;
};

//--------------------------------------------Functions Prototypes------------------------------------------------------

void * mainMMU();	// main MMU function
void * evicter();	// Maintain the memory when its full
void * printer();	// Handle MMU prints
void * hdFunction();	// Hard Disk function
void initVariables();
void killAll();

//---------------------------------------------Global Variables---------------------------------------------------------

pthread_mutexattr_t attr;
pthread_mutex_t memoryLock;	// Memory Mutex
int msqid[6];	// mailbox ID's
int memory[N];
int numPages = 0;	// pages counter
pthread_t hd;
pthread_t mmu;
pthread_t print;
pthread_t evict;

//-------------------------------------------------Functions------------------------------------------------------------

int main()
{

    initVariables();
    int memInvoke;
    double random_number = 0;

    //1. Process 1 + 2. Process 2 (Identical)

    pid_t pid[2];

    for(int i=0;i<2;i++)
    {
        pid[i] = fork();
        if(pid[i] == -1)	// Error occurred
        {
            perror("Error with pid %d\n");
            killAll();
            exit(EXIT_FAILURE);
        }
        else if(pid[i] == 0) // child proccess
        {
            int result;
            int processID;

            if(i==0)
                processID = PROCESS_1;
            else
                processID = PROCESS_2;

            while(1)
            {
                //1. Wait for INTER_MEM_ACCS_T [ns].
                sleep(INTER_MEM_ACCS_T*NANO);
                //2. Invoke a memory access, which is a write with probability 0 < WR_RATE < 1; and a read otherwise.
                random_number = (double)(rand() % (101))/100 ;
                if(random_number <=  WR_RATE)
                    memInvoke = WRITE;
                else
                    memInvoke = READ;

                //3. Send a request to the MMU (Memory Management Unit).

                struct buffer message;
                message.type = 1;
                message.source = processID;
                message.content = memInvoke;

                result = msgsnd(msqid[MMU], &message, sizeof(message), 0);	// Sends request to MMU's Mailbox
                if(result != 0)
                {
                    perror("MMU request ERROR\n");
                    killAll();
                    exit(EXIT_FAILURE);
                }

                //4. Wait for an ack from the MMU.
                result = 0;
                while(result == 0)	// Waits for ACK
                    result = msgrcv(msqid[processID], &message, sizeof(message), 0, 0);	// Reads from Process i's Mailbox
                if(result < 0)
                {
                    perror("Process failed receiving ACK from MMU\n");
                    killAll();
                    exit(EXIT_FAILURE);
                }

            }
        }
    }
    sleep(SIM_TIME);

    printf("Successfully finished sim");
    fflush(stdout);     //force to print the msg

    killAll();

    for(int i=0;i<2;i++)	// Since simulation is over, kill Processes
        kill(pid[i], SIGINT);

}
void *mainMMU()
{
    struct buffer message;
    struct buffer alarm;
    message.type = 1;
    int hOrM;	    // hOrM = HIT/MISS
    int error, randomPg, process, request ,result;
    double random_number = 0;
    double sleepTime;

    while(1)
    {
        result = 0;
        while(result == 0)
            result = msgrcv(msqid[MMU], &message, sizeof(message), 0, 0);
        if(result < 0)
        {
            perror("MMU failed to receive\n");
            killAll();
            exit(EXIT_FAILURE);
        }

        request = message.content;
        process = message.source;

        if(numPages)	// Memory isn't empty
        {
            random_number = (double)(rand() % (101))/100 ;
            hOrM = (random_number <= HIT_RATE) ? 1:0;
            if(hOrM == 1)
            {
                if(request == 1)
                {
                    //Sleep for MEM_WR_T [ns]
                    sleepTime = (double)MEM_WR_T*(double)NANO;
                    sleep(sleepTime);
                    //Choose uniformly at random one of the used pages in the memory, and mark it as dirty.
                    randomPg = (rand() % ((numPages - 1) + 1));
                    error = 16;
                    while(error == 16)
                        error = pthread_mutex_trylock(&memoryLock);
                    if(error)
                    {
                        printf("MMU failed to memoryLock mutex\n");
                        killAll();
                        exit(EXIT_FAILURE);
                    }
                    else
                        memory[randomPg] = 2;
                    error = pthread_mutex_unlock(&memoryLock); // Unlock mutex
                    if(error == 1)
                        printf("MMU tried to unlock mutex - but failed\n");

                }

                // Acknowledge the requesting process that the access was “done”.
                result = msgsnd(msqid[process], &message, sizeof(message), 0);
                if(result != 0)
                {
                    perror("MMU failed to send ACK to Process\n");
                    killAll();
                    exit(EXIT_FAILURE);
                }
            }
//          //In case of a miss (page fault)
            else if(hOrM == 0)
            {
                //If the memory is full
                if(numPages == N)
                {
                    // Wait until the evicter wakes me up again, indicating that the memory is not full anymore.

                    alarm.type = 1;
                    alarm.content = 3;
                    alarm.source = 3;
                    result = msgsnd(msqid[EVICTER], &alarm, sizeof(alarm), 0);
                    if(result != 0)
                    {
                        perror("MMU failed waking up Evicter\n");
                        killAll();
                        exit(EXIT_FAILURE);
                    }
                    result = 0;
                    while(result == 0)
                        result = msgrcv(msqid[MMU], &alarm, sizeof(alarm), 0, 0);
                    if(result < 0)
                    {
                        perror("MMU failed waking up Evicter\n");
                        killAll();
                        exit(EXIT_FAILURE);
                    }
                }
            }

        }
        if(numPages == 0 || hOrM == 0)
        {
            struct buffer req;
            req.type = 1;
            req.content = 3;
            req.source = MMU;
            //the thread sends the HD (hard disk) a request to read a page
            result = msgsnd(msqid[HD], &req, sizeof(req), 0);
            if(result != 0)
            {
                perror("MMU failed sending READ request to HD\n");
                killAll();
                exit(EXIT_FAILURE);
            }
            // Wait for ACK
            result = 0;
            while(result == 0)
                result = msgrcv(msqid[MMU], &req, sizeof(req), 0, 0);
            if(result < 0)
            {
                perror("MMU ACK failed from HD\n");
                killAll();
                exit(EXIT_FAILURE);
            }
            //After receiving an acknowledge from the HD
            error = 16;
            while(error == 16)	// Keep trying until mutex is opened
                error = pthread_mutex_trylock(&memoryLock);

            if(error)
            {
                printf("MMU mutex lock is failed\n");
                killAll();
                exit(EXIT_FAILURE);
            }
            else
            {
                memory[numPages] = 1;
                numPages = numPages + 1;
            }
            error = pthread_mutex_unlock(&memoryLock);
            if(error == 1)
            {
                printf("MMU mutex unlock is failed\n");
            }
            //the thread “writes” the page to the memory and acknowledges the requesting process, same as described
            // above in the case of a hit.
            message.content = ACK;
            message.source = MMU;
            result = 0;
            result = msgsnd(msqid[process], &message, sizeof(message), 0);
            if(result != 0)
            {
                perror("MMU failed to send ACK to Process\n");
                killAll();
                exit(EXIT_FAILURE);
            }
        }
    }

}
void *evicter()
{
    int result = 0;
    int sleepFLag = 1;	// sleep flag
    int error = 0;
    struct buffer alarm;
    alarm.type = 1;
    int i;

    while(1)
    {
        switch(sleepFLag)
        {
            case 1:		// case that its sleep
                result = 0;
                while(result == 0)
                    result = msgrcv(msqid[EVICTER], &alarm, sizeof(alarm), 0, 0);

                if(result < 0)
                {
                    perror("MMU fail to wakeup EVICTER\n");
                    exit(EXIT_FAILURE);
                }
                sleepFLag = 0;
                break;
            case 0:
                i = N-1;
                while(i != 1)
                {
                    error = 16;
                    while(error == 16)	// Keep trying until mutex is opened
                        error = pthread_mutex_trylock(&memoryLock);
                    if(error)
                    {
                        printf("mutex failed\n");
                        exit(EXIT_FAILURE);
                    }
                    else	//	Lock succeeded
                    {
                        memory[i] = 0;	// Write page to memory
                        numPages = numPages - 1;
                    }
                    error = pthread_mutex_unlock(&memoryLock); // Unlock mutex
                    if(error == 1)
                        printf("mutex unlock failed\n");
                    if(i == (N-1))	// i.e. after clearing 1st page
                    {
                        alarm.content = 3;	// Since logically only the Evicter can wake up the MMU
                        alarm.source = 3;
                        result = 0;
                        result = msgsnd(msqid[MMU], &alarm, sizeof(alarm), 0);
                        if(result !=0 )
                        {
                            perror("MMU fail to wake up EVICTER\n");
                            exit(EXIT_FAILURE);
                        }
                    }
                    i = i-1;
                }
                sleepFLag = 1;	// finished back to sleep
                break;

        }
    }
}
void *printer()
{
    //3.C. printer
    int error;
    int memoryCopy[N];

    while(1)
    {
        error = EBUSY;

        while(error == 16)	// Keep trying until mutex is opened
            error = pthread_mutex_trylock(&memoryLock);
        if(error)
        {
            printf("Failed lock mutex\n");
            killAll();
            exit(EXIT_FAILURE);
        }
        else	//	Lock succeeded
        {
            // CRITICAL SECTION
            for(int i=0;i<N;i++)
                memoryCopy[i] = memory[i];	// Copy memory into local variable
        }
        error = pthread_mutex_unlock(&memoryLock); // Unlock mutex
        if(error == 1)
            printf("Failed unlock mutex\n");

        // Printing loop
        for(int i=0;i<N;i++)
        {
            switch (memoryCopy[i]) {
                case 0:
                    printf("%d|-\n", i);
                    break;
                case 1:
                    printf("%d|0\n", i);
                    break;
                case 2:
                    printf("%d|1\n", i);
                    break;
                default:
                    printf("Error..");
                    killAll();
                    exit(EXIT_FAILURE);
            }
        }
        printf("\n\n");	// Spacing between prints

        // Sleep between prints
        double sleepTime;
        sleepTime = (double)TIME_BETWEEN_SNAPSHOTS*(double)NANO;
        sleep(sleepTime);
    }
}
void *hdFunction()
{
    int result;
    struct buffer message;
    message.type = 1;
    int requestMSG;

    while(1)
    {
        result = 0;
        //1. Receive requests.
        while(result == 0)
        {
            result = msgrcv(msqid[HD], &message, sizeof(message), 0, 0);	// Receive request
        }
        if(result<0)
        {
            perror("Error with HD request\n");
            killAll();
            exit(EXIT_FAILURE);
        }
        //2. Wait HD_ACCS_T [ns].
        else
            sleep(HD_ACCS_T*(double)NANO);

        //3. Send the requester an indication, that the request was “done”.
        requestMSG = message.source;
        message.source = HD;
        message.content = ACK;
        result = msgsnd(msqid[requestMSG], &message, sizeof(message), 0);
        if(result != 0 )
        {
            perror("Ack failed\n");
            killAll();
            exit(EXIT_FAILURE);
        }
    }
}
void initVariables()
{
    //Initialize mailbox, threads and mutex's

    for(int i=0;i<6;i++)                            //Initialize MailBox
        msqid[i] = msgget(i, 0600 | IPC_CREAT);

    if (pthread_mutexattr_init(&attr) != 0) 		// Init. attr as an official "pthread_mutex_attr"
    {
        perror("pthread_mutex_attr_init() error");
        exit(1);
    }

    if (pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK_NP) != 0)
    {
        perror("pthread_mutexattr_settype() error");
        exit(1);
    }

    if(pthread_mutex_init(&memoryLock, &attr) != 0 )  //Initialize mutex
    {
        perror("Error creating Mutex.\n Exiting Program.\n");
        exit(1);
    }


    if(pthread_create(&hd, NULL, hdFunction, NULL) != 0) //Initialize HD
    {
        perror("Error creating HD Thread exiting...");
        exit(1);
    }


    if(pthread_create(&mmu, NULL, mainMMU, NULL) != 0) //Initialize MMU
    {
        perror("Error creating MMU Thread exiting...");
        exit(1);
    }


    if(pthread_create(&evict, NULL, evicter, NULL) != 0)    //Initialize evicter
    {
        perror("Error creating Evicter Thread exiting...");
        exit(1);
    }


    if(pthread_create(&print, NULL, printer, NULL) != 0)    //Initialize printer
    {
        perror("Error creating Printer Thread exiting...");
        exit(1);
    }


}
void killAll()
{
    pthread_kill(print, SIGINT);	// Kill Printer
    pthread_kill(evict, SIGINT);	// Kill Evicter
    pthread_kill(mmu, SIGINT);		// Kill MMU
    pthread_kill(hd, SIGINT);		// Kill HD
    if(pthread_mutex_destroy(&memoryLock) != 0)	// Destroy the mutex "memoryLock"
    {
        perror("Destroy mutex failed\n");
        exit(EXIT_FAILURE);
    }
    // REmember to check if the mutex dies!
}
//----------------------------------------------------------------------------------------------------------------------