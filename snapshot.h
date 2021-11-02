#pragma once

#include <string>
#include <vector>

#include <iostream>
#include <fstream>

#include <unistd.h>
#include <pthread.h>

#include <aws/core/client/AsyncCallerContext.h>
#include <aws/ebs/EBSClient.h>

class Snapshot
{
    public:
        Snapshot(std::string snapshot, std::string outputFile);
        ~Snapshot();

        void execute(void);

        void getSnapshotBlockCallback(int blockIndex, Aws::EBS::Model::GetSnapshotBlockOutcome& outcome);

    private:
        std::string mSnapshot;
        long long mSize;

        std::fstream mOutputFile;

        Aws::EBS::EBSClient mClient;

        Aws::Vector<Aws::EBS::Model::Block> mBlocks;
        int mNumBlocks;
        int mCallbacksRecd;
        std::mutex mMtx;
        std::condition_variable mCond;
};

class SnapshotContext : public Aws::Client::AsyncCallerContext
{
    public:
        SnapshotContext(Snapshot* snapshot);
        ~SnapshotContext() = default;

        Snapshot* getSnapshot(void) const;

    private:
        Snapshot* mSnapshot;
};

/**
 * Process context to be passed as thread argument.
 */
struct ProcessContext
{
    pid_t pid;
};

class ThreadExecutor
{
    pthread_t mChildTID;
    void *mThreadArg;

    void *(*thread_routine) (void *);
    void setThreadAttributes(pthread_attr_t *attr);

  public:
    ThreadExecutor(void *(*start_routine) (void *), void*);

    void run();

    virtual ~ThreadExecutor() = default;
};

class ProcessExecutor
{
    Snapshot* mOperation;
    pid_t mChildPID;

    void deleteOperation();

  public:
    ProcessExecutor(Snapshot *op);

    void run();

    virtual ~ProcessExecutor();
};
