#pragma once

#include "FfmpegTypes.h"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace ffmpeg {

class FfmpegManager {
public:
    FfmpegManager();
    ~FfmpegManager();

    void Start();
    void Shutdown();

    void SetFfmpegPath(std::string path);
    bool IsFfmpegFound() const;

    std::uint64_t Enqueue(FfmpegRequest request);
    bool          CancelJob(std::uint64_t jobId);

    std::vector<FfmpegJob> SnapshotJobs() const;

private:
    void       WorkerLoop();
    void       ExecuteJob(std::uint64_t jobId);
    void       HandleOutputLine(FfmpegJob& job, const std::string& line);
    void       AppendLogLine(FfmpegJob& job, const std::string& line) const;
    FfmpegJob*       FindJobLocked(std::uint64_t jobId);
    const FfmpegJob* FindJobLocked(std::uint64_t jobId) const;
    bool TryGetJobRequestLocked(std::uint64_t jobId, FfmpegRequest& request) const;
    void RequestCancelCurrentProcess(std::uint64_t jobId);

    mutable std::mutex      mutex_;
    std::condition_variable cv_;
    std::vector<FfmpegJob>     jobs_;
    std::vector<FfmpegRequest> requests_;
    std::thread worker_;
    bool        running_  = false;
    bool        stopping_ = false;
    std::uint64_t nextJobId_ = 1;
    std::string ffmpegPath_;

    mutable std::mutex processMutex_;
    HANDLE        currentProcess_ = nullptr;
    std::uint64_t currentJobId_   = 0;
};

} 