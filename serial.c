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
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond1 = PTHREAD_COND_INITIALIZER;
int totalCompressFiles = 0, totalFiles = 0;

int totalInput = 0, TotalOut = 0, status = 1;
FILE *f_out; 

typedef struct CompressTask {
    int threadId;
    char *filePath;
} CompressTask ;


CompressTask taskQueue[256];
int taskCount = 0;

void compressFile(CompressTask cTask)
{
    // printf("compressing %d\n", cTask.threadId);
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

    while (totalCompressFiles != cTask.threadId) {
        pthread_cond_wait(&cond, &fileLock);
    }

    totalInput += nbytes;

    int nbytes_zipped = BUFFER_SIZE - strm.avail_out;
    TotalOut += nbytes_zipped;

    fwrite(&nbytes_zipped, sizeof(int), 1, f_out);
    fwrite(buffer_out, sizeof(unsigned char), nbytes_zipped, f_out);
    totalCompressFiles++;
    pthread_cond_broadcast(&cond);

    pthread_mutex_unlock(&fileLock);
    printf("file: %d total: %d\n", totalCompressFiles, totalFiles);
    printf("Compression rate: %.2lf%%\n", 100.0 * (totalInput - TotalOut) / totalInput);
    if(totalCompressFiles == totalFiles)
    {
        status = 0;
        pthread_cond_signal(&cond1);
    }
}

void* startThread(void *args)
{
    while (status == 1)
    {
        CompressTask CTask;

        pthread_mutex_lock(&fileLock);

        if(totalCompressFiles == totalFiles)
        {
            status = 0;
            break;
        }

        while (taskCount == 0)
        {
            pthread_cond_wait(&cond1, &fileLock);
        }
        

        CTask = taskQueue[0];
        for (int i = 0; i < taskCount - 1; i++)
        {
            taskQueue[i] = taskQueue[i + 1];
        }
        taskCount--;
        pthread_mutex_unlock(&fileLock);
        compressFile(CTask);
    }
    
}

void addTaskToQueue(CompressTask *cTask)
{
    pthread_mutex_lock(&fileLock);

    taskQueue[taskCount] = *cTask;
    taskCount++;

    pthread_mutex_unlock(&fileLock);
    pthread_cond_signal(&cond1);
}


// void *processPPM(void *ptr) {
//     struct threadDTO *trdData = (struct threadDTO *)ptr;
//     unsigned char buffer_in[BUFFER_SIZE];
//     unsigned char buffer_out[BUFFER_SIZE];

//     FILE *f_in = fopen(trdData->filePath, "r");
//     assert(f_in != NULL);
//     int nbytes = fread(buffer_in, sizeof(unsigned char), BUFFER_SIZE, f_in);
//     fclose(f_in);

//     // zip file
//     z_stream strm;
//     int ret = deflateInit(&strm, 9);
//     assert(ret == Z_OK);
//     strm.avail_in = nbytes;
//     strm.next_in = buffer_in;
//     strm.avail_out = BUFFER_SIZE;
//     strm.next_out = buffer_out;

//     ret = deflate(&strm, Z_FINISH);
//     assert(ret == Z_STREAM_END);

//     pthread_mutex_lock(&file_lock);

//     while (next_thread_id != trdData->threadId) {
//         pthread_cond_wait(&cond, &file_lock);
//     }

//     in += nbytes;

//     int nbytes_zipped = BUFFER_SIZE - strm.avail_out;
//     out += nbytes_zipped;

//     fwrite(&nbytes_zipped, sizeof(int), 1, f_out);
//     fwrite(buffer_out, sizeof(unsigned char), nbytes_zipped, f_out);
//     next_thread_id++;
//     pthread_cond_broadcast(&cond);

//     pthread_mutex_unlock(&file_lock);

//     pthread_exit(NULL);
// }

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
    totalFiles = nfiles;
    pthread_t trdPool[THREAD_POOL_LIMIT];

    for (int i = 0; i < THREAD_POOL_LIMIT; i++)
    {
        pthread_create(&trdPool[i], NULL, &startThread, NULL);
    }
    
    int threadCounter = 0;
    for (int i = 0; i < nfiles; i++) {
        int len = strlen(argv[1]) + strlen(files[i]) + 2;
        char *full_path = malloc(len * sizeof(char));
        assert(full_path != NULL);
        strcpy(full_path, argv[1]);
        strcat(full_path, "/");
        strcat(full_path, files[i]);

        struct CompressTask *trdData = malloc(sizeof(struct CompressTask));
        trdData->threadId = i;
        trdData->filePath = strdup(full_path);

        // pthread_create(&trdPool[threadCounter], NULL, processPPM, (void *)trdData);
        addTaskToQueue(trdData);
        threadCounter++;

        // if (threadCounter == THREAD_LIMIT) {
        //     for (int j = 0; j < THREAD_LIMIT; j++) {
        //         pthread_join(tid[j], NULL);
        //     }
        //     threadCounter = 0;
        // }

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

    // pthread_mutex_destroy(&lock);

    // do not modify the main function after this point!

    // time computation footer
    clock_gettime(CLOCK_MONOTONIC, &end);
    printf("Time: %.2f seconds\n", ((double)end.tv_sec+1.0e-9*end.tv_nsec)-((double)start.tv_sec+1.0e-9*start.tv_nsec));
    // end of time computation footer

    return 0;
}
