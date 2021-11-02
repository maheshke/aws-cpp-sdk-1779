#pragma once

#include <string>
#include <vector>

#include <iostream>
#include <fstream>

#include <unistd.h>

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
