/*
 * Demo of a Handler/Session/Processor design implementation.
 *
 * ```
 * $ clang++-20 -std=c++17 test.cpp && ./a.out
 * Usage: Press a command letter, followed by <Enter>
 *   'b' -> Begin a new session
 *   'e' -> End current session
 *   'l' -> Load surface
 *   'u' -> Update layers
 *   'g' -> Get preview points
 *   'c' -> Create design
 *   'h' -> Print this help message
 * ```
 * Only b, e, l and h are implemented. This shoul'd be enough to evaluate the implementation.
 * Edge cases can be tested by sending b, e and l commands in quick successive random order. 
 */

#include <atomic>
#include <chrono>
#include <future>
#include <iomanip>
#include <iostream>
#include <list>
#include <memory>
#include <queue>
#include <random>
#include <thread>

#include <poll.h>
#include <unistd.h>
#include <cerrno>

// Logging function that prints millisecond timestamp, caller's thread ID, `this` and a log message
#define _STAMP std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()
#define _TID std::this_thread::get_id()
#define _THIS std::hex << std::uppercase << std::setfill('0') << this << std::dec
#define LOG(message) do { std::cout << _STAMP << "[" << _TID << "][" << _THIS << "] "<< __PRETTY_FUNCTION__ << " | " << message << std::flush << std::endl; } while (0)
#define LOG_ENTER() LOG("enter")
#define LOG_EXIT() LOG("exit")
#define LOG_NOT_IMPLEMENTED() LOG("NOT IMPLEMENTED")

// Convenience random number generator
int random_int(int low, int high)
{
    static thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<int> dist(low, high);
    return dist(rng);
}

// Dummy types
struct SurfaceData {};
struct Mesh {};
struct LayerSettings {};

// The Processor class encapsulates all the mesh related operations
// Operations are cancellable
class Processor
{
  std::atomic<bool> m_CancelRequested{false};
  
public:
  Processor()
  {
    LOG("");
  }

  ~Processor()
  {
    LOG("");
  }

  // Mesh processing implementation goes here, to be used by the Session class
  //
  // ...
  //

  void Cancel()
  {
    m_CancelRequested = true;
  }

  bool WasCancelled() const
  {
    return m_CancelRequested;
  }
  
  // Simulate a processing step that takes a few seconds to execute
  // and that handle cancellation
  void DoStuff()
  {
    for (int i=0; i<100; ++i) {
      if (m_CancelRequested) {
	return;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(random_int(15, 30)));
    }
  }

};

// The Session class provides an interface for executing lift layers operations.
// It holds all the lift layer data, while relying on a Processor object to execute the operations asynchrounously. 
class Session
{
public:
  Session(std::unique_ptr<Processor> processor):
    m_processor(std::move(processor))
  {
    LOG("");
  }

  ~Session()
  {
    LOG("");
  }

  int LoadSurface(int arg, std::function<void(const Session*)> callback)
  {
    LOG_ENTER();
    auto future_result = std::async(std::launch::async, [this, callback, arg]() {
      m_processor->DoStuff(); // Should call real operations with parameters, ...
      // Do not call the callback if we were cancelled while the operation was in progress
      if (not m_processor->WasCancelled()) {
	callback(this); // `this` can be used in the callback to access current session data
      }
    });
    // Enqueue the future so that it's dtor doesn't block us here
    // And we can clean it up later when the work is done or is cancelled
    m_PendingFutures.push_back(std::move(future_result));
    LOG_EXIT();
    return 0;
  }
  
  int UpdateLayers()
  {
    LOG_NOT_IMPLEMENTED();
    return 0;
  }
  
  int GetPreviewPoints()
  {
    LOG_NOT_IMPLEMENTED();
    return 0;
  }
  
  int CreateDesign()
  {
    LOG_NOT_IMPLEMENTED();
    return 0;
  }

  void Cancel()
  {
    LOG_ENTER();
    // Simply forward to the mesh processing object
    m_processor->Cancel();
    LOG_EXIT();
  }

  bool HasPendingOperations()
  {
    return not m_PendingFutures.empty();
  }

  void CheckPendingOperations()
  {
    for (auto it = m_PendingFutures.begin(); it != m_PendingFutures.end(); ) {
      if (not it->valid()) {
	// No shared state, it's safe to ignore
	LOG("ERASE INVALID");
	it = m_PendingFutures.erase(it);
      }
      else if (it->wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
	// Operation is done, get rid of it
	LOG("ERASE DONE");
	it = m_PendingFutures.erase(it);
      }
      else {
	// Still pending, keep it and check the next one
	LOG("KEEP");
	++it;
      }
    }
  }

private:
  std::unique_ptr<Processor> m_processor;
  std::list<std::future<void>> m_PendingFutures;

  // These are the session data, as per Matthew document
  // Some need to be exposed so that the Mosaic handler can return them to the UI
  SurfaceData m_CriticalSurfaceData;
  Mesh m_CriticalMesh;

  SurfaceData m_CutSurfaceData;
  LayerSettings m_CutLayerSettings;
  Mesh m_CutMesh;
  std::list<Mesh> m_CutLayers;
  
  SurfaceData m_FillSurfaceData;
  LayerSettings m_FillLayerSettings;
  Mesh m_FillMesh;
  std::list<Mesh> m_FillLayers;


  // Simulate a processing step that takes between 1 and 2 seconds to execute
  void DoStuff()
  {
    for (int i=0; i<100; ++i) {
      std::this_thread::sleep_for(std::chrono::milliseconds(random_int(10, 20)));
    }
  }
};


// A MOC of the Mosaic component
// The role is to handle requests while delegating the business logic to the session class
class MosaicComponent
{
public:
  
  void HandleBeginSessionRequest()
  {
    LOG_ENTER();
    if (m_CurrentSession) {
      DiscardCurrentSession();
    }
    auto processor = std::make_unique<Processor>();
    m_CurrentSession = std::make_unique<Session>(std::move(processor));
    SendSuccessResponse("Session started");
    LOG_EXIT();
  }

  void HandleEndSessionRequest()
  {
    LOG_ENTER();
    if (!m_CurrentSession) {
      SendErrorResponse("No active session");
      return;
    }
    DiscardCurrentSession();
    SendSuccessResponse("Session stopped");
    LOG_EXIT();
  }

  void HandleLoadSurfaceRequest()
  {
    LOG_ENTER();
    if (!m_CurrentSession) {
      SendErrorResponse("No active session");
      return;
    }
    if (m_CurrentSession->HasPendingOperations()) {
      SendErrorResponse("Operation already in progress");
      return;
    }
    const int arg = 42; // request.arg
    m_CurrentSession->LoadSurface(arg, [this] (const Session *session) -> void {
      // data = session->GetSomeData()
      // response.data = data
      SendSuccessResponse("Surface loaded");
    });
    LOG_EXIT();
  }

  void HandleUpdateLayersRequest()
  {
    LOG_NOT_IMPLEMENTED();
  }
  
  void HandleGetPreviewPointsRequest()
  {
    LOG_NOT_IMPLEMENTED();
  }

  void HandleCreateDesignRequest()
  {
    LOG_NOT_IMPLEMENTED();
  }

  void HandlePeriodicTasks()
  {
    // Cleanup all futures to free resources
    // 1. Current session
    if (m_CurrentSession) {
      m_CurrentSession->CheckPendingOperations();
    }
    // 2. All discarded sessions
    for (auto it = m_DiscardedSessions.begin(); it != m_DiscardedSessions.end(); ) {
      (*it)->CheckPendingOperations();
      if ((*it)->HasPendingOperations()) {
	LOG("KEEP");
	++it;
      }
      else {
	LOG("ERASE DONE");
	it = m_DiscardedSessions.erase(it);
      }
    }    
  }
  
private:
  void SendErrorResponse(const std::string &message)
  {
    LOG(message);
  }

  void SendSuccessResponse(const std::string &message)
  {
    LOG(message);
  }

  void DiscardCurrentSession()
  {
    if (m_CurrentSession->HasPendingOperations()) {
      m_DiscardedSessions.push_back(std::move(m_CurrentSession));
    }
    else {
      m_CurrentSession.reset();
    }
	
  }
  
  std::unique_ptr<Session> m_CurrentSession;
  std::list<std::unique_ptr<Session>> m_DiscardedSessions;
};


void print_usage()
{
  std::cout << "Usage: Press a command letter, followed by <Enter>\n";
  std::cout << " 'b' -> Begin a new session\n";
  std::cout << " 'e' -> End current session\n";
  std::cout << " 'l' -> Load surface\n";
  std::cout << " 'u' -> Update layers\n";
  std::cout << " 'g' -> Get preview points\n";
  std::cout << " 'c' -> Create design\n";
  std::cout << " 'h' -> Print this help message\n";
}

int main(int, char**)
{
  constexpr int timeout_ms = 100;

  pollfd pfd{};
  pfd.fd = STDIN_FILENO;
  pfd.events = POLLIN;

  print_usage();

  MosaicComponent component;
  bool running = true;
  while (running) {
    int rc = ::poll(&pfd, 1, timeout_ms);
    if (rc < 0) {
      std::perror("poll");
      break;
    }
    if (rc == 0) {
      component.HandlePeriodicTasks();
    }
    else {
      char command;
      ssize_t n = ::read(STDIN_FILENO, &command, 1);
      if (n < 1) {
	break; // Error or EOF
      }
      switch(command) {
      case 'b':
	component.HandleBeginSessionRequest();
	break;
      case 'l':
	component.HandleLoadSurfaceRequest();
	break;
      case 'u':
	component.HandleUpdateLayersRequest();
	break;
      case 'g':
	component.HandleGetPreviewPointsRequest();
	break;
      case 'c':
	component.HandleCreateDesignRequest();
	break;
      case 'e':
	component.HandleEndSessionRequest();
	break;
      case 'h':
	print_usage();
	break;
      default:
	break;
      }
    }
    pfd.revents = 0;    
  }
  return 0;
}
