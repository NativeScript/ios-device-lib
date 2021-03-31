#include "SetTimeout.h"
#include "Constants.h"
#include <mutex>
#include <thread>

#ifdef WIN32
DWORD WINAPI TimeoutThreadExecutor(LPVOID lpParam) {
  TimeoutData *threadData = (TimeoutData *)lpParam;
  std::this_thread::sleep_for(
      std::chrono::milliseconds(threadData->timeout * 1000));
  threadData->operation(threadData->data);
  return 0;
}
#endif

TimeoutOutputData *setTimeout(int timeout, void *data,
                              void (*operation)(void *)) {
  std::thread::native_handle_type darwinHandle = nullptr;
  HANDLE windowsHandle = nullptr;
  struct TimeoutData *timeoutData = nullptr;

  if (timeout > 0) {
    timeoutData =
        reinterpret_cast<TimeoutData *>(malloc(sizeof(struct TimeoutData)));
    timeoutData->timeout = timeout;
    timeoutData->data = data;
    timeoutData->operation = operation;

#ifdef WIN32
    windowsHandle = CreateThread(NULL, // default security attributes
                                 0,    // use default stack size
                                 TimeoutThreadExecutor, // thread function name
                                 timeoutData, // argument to thread function
                                 0,           // use default creation flags
                                 nullptr);
#else
    std::thread thread = std::thread([=]() {
      std::this_thread::sleep_for(
          std::chrono::milliseconds(timeoutData->timeout * 1000));
      timeoutData->operation(timeoutData->data);
    });

    darwinHandle = thread.native_handle();
    thread.detach();
#endif
  }

  TimeoutOutputData *result = reinterpret_cast<TimeoutOutputData *>(
      malloc(sizeof(struct TimeoutOutputData)));
  result->windowsHandle = windowsHandle;
  result->darwinHandle = darwinHandle;
  result->timeoutData = timeoutData;

  return result;
}

void clearTimeout(TimeoutOutputData *data) {
  if (data->timeoutData) {
    free(data->timeoutData);
    data->timeoutData = nullptr;
  }

#ifdef WIN32
  if (data->windowsHandle) {
    int terminateThreadResult = TerminateThread(data->windowsHandle, 9);
    // https://docs.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-terminatethread#return-value
    if (terminateThreadResult == 0) {
      DWORD errorCode = GetLastError();
    }
  }
#else
  if (data->darwinHandle) {
    pthread_cancel(data->darwinHandle);
  }
#endif

  free(data);
  data = nullptr;
}
