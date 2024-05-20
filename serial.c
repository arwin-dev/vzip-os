#include <dirent.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include <time.h>
#include <pthread.h>

#define BUFFER_SIZE 1048576 // 1MB
#define THREAD_POOL_LIMIT 19 // 19 + 1 = 20

int cmp(const void *a, const void *b) {
    return strcmp(*(char **)a, *(char **)b);
}

pthread_mutex_t fileLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t queueLock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond1 = PTHREAD_COND_INITIALIZER;

int totalFIleTracker = 0, totalFiles = 0, totalInput = 0, TotalOut = 0;
FILE *f_out;

typedef struct CompressTask {
    int threadId;
    char *filePath;
} CompressTask;

// Declare taskQueue as a pointer
CompressTask *taskQueue = NULL; 
int taskCount = 0;
int taskQueueSize = 256; // Initial size

// Resizes the Queue size
void resizeTaskQueue(int newSize) {
    taskQueue = (CompressTask *)realloc(taskQueue, newSize * sizeof(CompressTask));
    if (taskQueue == NULL) {
        perror("Error reallocating memory for taskQueue");
        exit(EXIT_FAILURE);
    }
    taskQueueSize = newSize;
}

// Function that compresses task and writes to the file
void compressFile(CompressTask cTask) {
    unsigned char buffer_in[BUFFER_SIZE];
    unsigned char buffer_out[BUFFER_SIZE];

    FILE *f_in = fopen(cTask.filePath, "r");
    assert(f_in != NULL);
    int nbytes = fread(buffer_in, sizeof(unsigned char), BUFFER_SIZE, f_in);
    fclose(f_in);

    // zip file
    z_stream strm;
    int ret = deflateInit(&strm, 9);
    assert(ret == Z_OK);
    strm.avail_in = nbytes;
    strm.next_in = buffer_in;
    strm.avail_out = BUFFER_SIZE;
    strm.next_out = buffer_out;

    ret = deflate(&strm, Z_FINISH);
    assert(ret == Z_STREAM_END);

    pthread_mutex_lock(&fileLock);

    // checks if the thread is in lexological order. If not, puts the threat to sleep.
    while (totalFIleTracker != cTask.threadId) {
        pthread_cond_wait(&cond, &fileLock);
    }

    totalInput += nbytes;

    int nbytes_zipped = BUFFER_SIZE - strm.avail_out;
    TotalOut += nbytes_zipped;

    fwrite(&nbytes_zipped, sizeof(int), 1, f_out);
    fwrite(buffer_out, sizeof(unsigned char), nbytes_zipped, f_out);
    totalFIleTracker++;

    // Wakes up all the threads that are waiting in line 70
    pthread_cond_broadcast(&cond);

    pthread_mutex_unlock(&fileLock);

    if (totalFIleTracker == totalFiles) {
        //If all the files are processed, wakes up all the sleeping threads
        pthread_cond_broadcast(&cond1);
        pthread_exit(NULL); 
    }
}

// Initializes the thread and processes the Task
void *startThread(void *args) {
    while (1) {
        pthread_mutex_lock(&queueLock);

        // If the queue doesn't have any available Task, puts the thread to sleep
        while (taskCount == 0 && totalFIleTracker < totalFiles) {
            pthread_cond_wait(&cond1, &queueLock);
        }

        // If all the files are processed, exits the loop
        if (taskCount == 0 && totalFIleTracker == totalFiles) {
            pthread_mutex_unlock(&queueLock);
            break; // No more tasks and all files compressed, exit the thread
        }

        // FIFO - Gets the first Task in the queue and compresses the Task
        CompressTask cTask = taskQueue[0];
        for (int i = 0; i < taskCount - 1; i++) {
            taskQueue[i] = taskQueue[i + 1];
        }
        taskCount--;

        pthread_mutex_unlock(&queueLock);
        
        //Process the Task
        compressFile(cTask);
    }

    pthread_exit(NULL);
}

// Adds a Task into taskQueue to be compressed 
void addTaskToQueue(CompressTask *cTask) {
    pthread_mutex_lock(&queueLock);

    if (taskCount == taskQueueSize) {
        // Resize taskQueue if it's full
        resizeTaskQueue(taskQueueSize * 2);
    }

    taskQueue[taskCount] = *cTask;
    taskCount++;

    pthread_mutex_unlock(&queueLock);
    pthread_cond_signal(&cond1); // Signal a task is added
}

int main(int argc, char **argv) {
    // time computation header
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    // end of time computation header

    // do not modify the main function before this point!

    assert(argc == 2);

    DIR *d;
    struct dirent *dir;
    char **files = NULL;
    int nfiles = 0;

    d = opendir(argv[1]);
    if (d == NULL) {
        printf("An error has occurred\n");
        return 0;
    }

    // create sorted list of PPM files
    while ((dir = readdir(d)) != NULL) {
        files = realloc(files, (nfiles + 1) * sizeof(char *));
        assert(files != NULL);

        int len = strlen(dir->d_name);
        if (dir->d_name[len - 4] == '.' && dir->d_name[len - 3] == 'p' && dir->d_name[len - 2] == 'p' &&
            dir->d_name[len - 1] == 'm') {
            files[nfiles] = strdup(dir->d_name);
            assert(files[nfiles] != NULL);

            nfiles++;
        }
    }
    closedir(d);
    qsort(files, nfiles, sizeof(char *), cmp);

    // create a single zipped package with all PPM files in lexicographical order
    f_out = fopen("video.vzip", "w"); // Open output file once

    // Sets the max files
    totalFiles = nfiles;

    // Creates a thread pool with THREAD_POOL_LIMIT as the limit
    pthread_t trdPool[THREAD_POOL_LIMIT];

    // Initializes the threads in the thread pool
    for (int i = 0; i < THREAD_POOL_LIMIT; i++) {
        pthread_create(&trdPool[i], NULL, &startThread, NULL);
    }

    // Allocate memory for taskQueue
    taskQueue = (CompressTask *)malloc(taskQueueSize * sizeof(CompressTask));

    if (taskQueue == NULL) {
        perror("Error allocating memory for taskQueue");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < nfiles; i++) {
        int len = strlen(argv[1]) + strlen(files[i]) + 2;
        char *full_path = malloc(len * sizeof(char));
        assert(full_path != NULL);
        strcpy(full_path, argv[1]);
        strcat(full_path, "/");
        strcat(full_path, files[i]);

        // Initilizes a Task and sets initial values to it
        CompressTask *trdData = malloc(sizeof(CompressTask));
        trdData->threadId = i;
        trdData->filePath = strdup(full_path);

        // Adds the Task into the queue
        addTaskToQueue(trdData);

        free(full_path);
    }

    for (int j = 0; j < THREAD_POOL_LIMIT; j++) {
        pthread_join(trdPool[j], NULL);
    }

    fclose(f_out); // Close output file after all threads finish

    printf("Compression rate: %.2lf%%\n", 100.0 * (totalInput - TotalOut) / totalInput);

    // release list of files
    for (int i = 0; i < nfiles; i++)
        free(files[i]);
    free(files);
    free(taskQueue); // Free dynamically allocated memory for taskQueue

    // do not modify the main function after this point!

    // time computation footer
    clock_gettime(CLOCK_MONOTONIC, &end);
    printf("Time: %.2f seconds\n", ((double)end.tv_sec + 1.0e-9 * end.tv_nsec) - ((double)start.tv_sec + 1.0e-9 * start.tv_nsec));
    // end of time computation footer

    return 0;
}