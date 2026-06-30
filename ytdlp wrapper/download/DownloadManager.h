#pragma once

#include "../core/DownloadTypes.h"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace download {

struct DependencyStatus {
    bool ytdlpFound = false;
    bool ffmpegFound = false;
    std::string resolvedYtDlpPath;
};

class DownloadManager {
public:
    DownloadManager();
    ~DownloadManager();

    void Start();
    void Shutdown();

    void SetYtDlpPath(std::string path);
    DependencyStatus RefreshDependencies();
    DependencyStatus GetDependencies() const;

    std::uint64_t Enqueue(core::DownloadRequest request);
    bool CancelJob(std::uint64_t jobId);
    bool PauseJob(std::uint64_t jobId);
    bool ResumeJob(std::uint64_t jobId);

    std::vector<core::DownloadJob> SnapshotJobs() const;

private:
    void WorkerLoop();
    void ExecuteJob(std::uint64_t jobId);
    void HandleOutputLine(core::DownloadJob& job, const std::string& line) const;
    void AppendLogLine(core::DownloadJob& job, const std::string& line) const;
    core::DownloadJob* FindJobLocked(std::uint64_t jobId);
    const core::DownloadJob* FindJobLocked(std::uint64_t jobId) const;
    bool TryGetJobRequestLocked(std::uint64_t jobId, core::DownloadRequest& request) const;
    void RequestCancelCurrentProcess(std::uint64_t jobId);

    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<core::DownloadJob> jobs_;
    std::vector<core::DownloadRequest> requests_;
    std::thread worker_;
    bool running_ = false;
    bool stopping_ = false;
    std::uint64_t nextJobId_ = 1;
    std::string ytDlpPath_;
    DependencyStatus dependencies_;

    mutable std::mutex processMutex_;
    HANDLE currentProcess_ = nullptr;
    std::uint64_t currentJobId_ = 0;
};

} 