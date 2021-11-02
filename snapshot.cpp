#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>
#include <assert.h>

#include "snapshot.h"

#include <aws/ebs/model/ListSnapshotBlocksRequest.h>
#include <aws/ebs/model/GetSnapshotBlockRequest.h>

#define KB (1024)
#define GB (KB * KB * KB)

using namespace std;

ProcessExecutor::ProcessExecutor(Snapshot *pOperation):
    mOperation(pOperation)
{
}

void ProcessExecutor::deleteOperation()
{
  delete mOperation;
  mOperation = NULL;
}

void ProcessExecutor::run()
{
  // Creating new process to make the call async.
  printf("Process executor is executing....\n");
  assert(mOperation != NULL);

  if((mChildPID = fork()) < 0)
  {
    switch(mChildPID)
    {
      const char *msg;
      case EAGAIN:
        msg = "Error while creating new process. System limit reached.";
        printf(msg);
        throw std::runtime_error(msg);
      case ENOMEM:
        msg = "Error while creating new process. Memory is tight.";
        printf(msg);
        throw std::runtime_error(msg);
      default:
        msg = "Error while creating new process";
        printf("%s: %d.", msg, mChildPID);
        throw std::runtime_error(msg);
    }
  }
  if(mChildPID > 0)
  {
    // Parent
  }
  else
  {
    int exitStatus = -1;

    // New child process.
    try
    {
      /*
      * modify the nice value (process priority) so that the parent runs at high priority
      * and the child operation runs at lower priority
      */

      errno = 0;

      int niceReturnValue = nice(10);
      if(niceReturnValue == -1 && errno != 0)
      {
        printf("Failed to lower the priority of the process %d", getpid());
      }
      else
      {
        printf("Reduced the priority of the process %d\n", getpid());
      }

      mOperation->execute();

      exitStatus = 0;
    } catch(std::exception &e) {
      printf("Exception propagated to process executor: %s.", e.what ());
      exitStatus = -1;
    }

    /* Destruct the operation object in the child process */
    deleteOperation();

    exit(exitStatus);
  }

  // wait for child
  {
    int status;

    if(waitpid(mChildPID, &status, WUNTRACED | WCONTINUED) == -1)
    {
        const char *msg = "Error while waiting for new process.";
        printf("Errno on waitpid %d.\n", errno);
        printf(msg);
    }

    if (WIFEXITED(status)) {
        printf("Process %d exited with status: %d.\n", mChildPID, WEXITSTATUS(status));
    }
    else if (WIFSIGNALED(status)) {
        printf("Process %d killed by signal: %d.\n", mChildPID, WTERMSIG(status));
    }

    printf("Child %d exited.\n", mChildPID);
  }
}

ProcessExecutor::~ProcessExecutor ()
{
  if(mOperation != NULL)
  {
    deleteOperation();
  }
}

static void getSnapshotBlockCallback(const Aws::EBS::EBSClient* client,
        const Aws::EBS::Model::GetSnapshotBlockRequest& request,
        Aws::EBS::Model::GetSnapshotBlockOutcome outcome,
        const std::shared_ptr<const Aws::Client::AsyncCallerContext>& ctx)
{
    (void)client;

    // Get Snapshot from ctx and call callback defined in Snapshot
    std::shared_ptr<const SnapshotContext> snapshotCtx =
        std::dynamic_pointer_cast<const SnapshotContext>(ctx);

    auto snapshot = snapshotCtx->getSnapshot();
    snapshot->getSnapshotBlockCallback(request.GetBlockIndex(), outcome);
}

/* Snapshot methods */
Snapshot::Snapshot(string snapshot, string outputFile) :
    mSnapshot(snapshot),
    mSize(),
    mBlocks(),
    mNumBlocks(),
    mCallbacksRecd(0),
    mMtx(),
    mCond()
{
    mOutputFile.open(outputFile, ios::out | ios::binary);

    if (!mOutputFile.is_open())
    {
        printf("Failed to open %s\n", outputFile.c_str());
        throw std::runtime_error("File opening failed");
    }

    Aws::Client::ClientConfiguration config;

    mClient = Aws::EBS::EBSClient(config);
}

Snapshot::~Snapshot()
{
    mOutputFile.close();
}

void Snapshot::execute(void)
{
    // Get list of allocated blocks
    bool done = false;

    Aws::EBS::Model::ListSnapshotBlocksRequest request;
    request.SetSnapshotId(mSnapshot);
    request.SetMaxResults(10000);

    while(!done)
    {
        auto outcome = mClient.ListSnapshotBlocks(request);

        if (!outcome.IsSuccess())
        {
            auto err = outcome.GetError();

            cout << "Error: ListSnapshotBlocks: " << mSnapshot <<
                err.GetExceptionName() << ": " << err.GetMessage() << std::endl;

            return;
        }

        mSize = outcome.GetResult().GetVolumeSize();

        mBlocks.insert(mBlocks.end(), outcome.GetResult().GetBlocks().begin(),
                outcome.GetResult().GetBlocks().end());

        cout << "ListSnapshotBlocks: " << mSnapshot <<" received " <<
            outcome.GetResult().GetBlocks().size() << " blocks. Next token " <<
            outcome.GetResult().GetNextToken() << ", total blocks " <<
            mBlocks.size() << " snapshot size " << mSize << "GB" << std::endl;

        const auto &next_token = outcome.GetResult().GetNextToken();
        request.SetNextToken(next_token);
        done = next_token.empty();
    }

    cout << "ListSnapshotBlocks completed for " << mSnapshot << ", total blocks " <<
        mBlocks.size() << " size: " << mSize << "GB" << std::endl;

    // All the allocated blocks are now available in mBlocks
    mNumBlocks = mBlocks.size();

    // Queue all the GetSnapshotBlockRequest s with SDK
    auto ctx = std::make_shared<SnapshotContext>(this);
    auto count = 0;
    auto totalCB = 0;

    for(auto& block: mBlocks)
    {
        Aws::EBS::Model::GetSnapshotBlockRequest request;
        request.SetSnapshotId(mSnapshot);
        request.SetBlockIndex(block.GetBlockIndex());
        request.SetBlockToken(block.GetBlockToken());

        mClient.GetSnapshotBlockAsync(request, ::getSnapshotBlockCallback, ctx);

        count++;

        if (count == 40)
        {
            cout << "Received " << totalCB << " callbacks" << std::endl;

            std::unique_lock<std::mutex> lock(mMtx);

            while(mCallbacksRecd != count)
            {
                mCond.wait(lock);
            }

            totalCB += mCallbacksRecd;

            mCallbacksRecd = 0;
            count = 0;
        }
    }

    cout << "Received " << totalCB << " callbacks" << std::endl;

    if (count)
    {
        std::unique_lock<std::mutex> lock(mMtx);

        while(mCallbacksRecd != count)
        {
            mCond.wait(lock);
        }

        totalCB += mCallbacksRecd;
    }

    cout << "Received " << totalCB << " callbacks" << std::endl;
    cout << "Finished writing " << mNumBlocks << " blocks" << endl;
}

void Snapshot::getSnapshotBlockCallback(int blockIndex, Aws::EBS::Model::GetSnapshotBlockOutcome& outcome)
{
    if(outcome.IsSuccess())
    {
        auto dataLength = outcome.GetResult().GetDataLength();

        char buffer[dataLength];
        (void)outcome.GetResult().GetBlockData().read(buffer, dataLength);

        {
            std::lock_guard<std::mutex> lock(mMtx);
            mCallbacksRecd++;

            mOutputFile.seekp(blockIndex * 512 * KB);
            mOutputFile.write(buffer, dataLength);
            mOutputFile.flush();
        }
    }
    else
    {
        auto err = outcome.GetError();

        cout << "Error: GetSnapshotBlock: " << blockIndex <<
            err.GetExceptionName() << ": " << err.GetMessage() << std::endl;

        std::lock_guard<std::mutex> lock(mMtx);
        mCallbacksRecd++;
    }

    mCond.notify_one();
}

/* SnapshotContext methods */
SnapshotContext::SnapshotContext(Snapshot* snapshot) :
    mSnapshot(snapshot)
{
}

Snapshot* SnapshotContext::getSnapshot(void) const
{
    return mSnapshot;
}
