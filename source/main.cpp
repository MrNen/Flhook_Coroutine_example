#include <iostream>
#include <coroutine>
#include <thread>
#include <queue>
#include <mutex>
#include <future>
#include "concurrentqueue/concurrentqueue.h"

enum class Status {
  FLHOOK_AWAIT,
  DATABASE_AWAIT,
  FINISHED,
  ERROR,
  INIT
};

struct Promise;

Status getStatusFromPromise(Promise &p);

class [[nodiscard]]Task {
 public:
  Status status = Status::INIT;

  using promise_type = Promise;

  explicit Task(std::coroutine_handle<Promise> h)
	  : handle(h) {
	updateStatus();
  }

  void run() {
	if (status == Status::FINISHED) {
	  handle.destroy();
	  return;
	}
	handle.resume();
	updateStatus();
  }

  void updateStatus(){
	status = getStatusFromPromise(handle.promise());
  }

 private:
  std::coroutine_handle<Promise> handle;

};

struct Promise {
  const Status getStatus() {
	return value_;
  }
  Status value_;
  std::suspend_never initial_suspend() const noexcept { return {}; }
  std::suspend_always final_suspend() const noexcept { return {}; }

  Task get_return_object() {
	return Task(std::coroutine_handle<Promise>::from_promise(*this));
  }

  void return_value(Status &&newStatus) {
	value_ = std::forward<Status>(newStatus);
  }

  std::suspend_always yield_value(Status &&newStatus) {
	value_ = std::forward<Status>(newStatus);
	return {};
  }

  void unhandled_exception() noexcept {
	std::cerr << "Unhandled exception caught...\n";
	exit(1);
  }
};

inline Status getStatusFromPromise(Promise &p) {
  return p.getStatus();
}

class TaskScheduler {
  moodycamel::ConcurrentQueue<std::shared_ptr<Task>> mainTasks;
  moodycamel::ConcurrentQueue<std::shared_ptr<Task>> dataBaseTasks;
  std::jthread mongoThread;

  void RunDBThreadTasksImpl() {
	for (size_t i = dataBaseTasks.size_approx(); i > 0; i--) {
	  std::shared_ptr<Task> t;
	  if (!dataBaseTasks.try_dequeue(t)) {
		continue;
	  }
	  t->run();
	  auto status = t->status;
	  if (status == Status::FLHOOK_AWAIT) {
		mainTasks.try_enqueue(t);
	  } else if (status == Status::DATABASE_AWAIT) {
		dataBaseTasks.try_enqueue(t);
	  }
	}
  }

 public:
  void RunMainThreadTasks() {
	for (size_t i = mainTasks.size_approx(); i > 0; i--) {
	  std::shared_ptr<Task> t;
	  if (!mainTasks.try_dequeue(t)) {
		continue;
	  }
	  t->run();
	  auto status = t->status;
	  if (status == Status::FLHOOK_AWAIT) {
		mainTasks.try_enqueue(t);
	  } else if (status == Status::DATABASE_AWAIT) {
		dataBaseTasks.try_enqueue(t);
	  }
	}
  };

  void runDatabaseThreadTasks() {
	mongoThread = std::jthread(&TaskScheduler::RunDBThreadTasksImpl, this);
  }

  bool addTask(const std::shared_ptr<Task> &t) {
	return mainTasks.try_enqueue(t);
  }

};




class Bar{
 public:
Task foo(int delay) {
  co_yield Status::FLHOOK_AWAIT;
  std::cout << "Hello from FlHook.\n";
  co_yield Status::DATABASE_AWAIT;
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  std::cout << "Hello from Mongo.\n";
  co_yield Status::FLHOOK_AWAIT;
  std::cout << "Hello from FlHook again.\n";
  co_return Status::FINISHED;
}

Task bar(){
  std::cout << "Immediately do the thing.\n";
  co_return Status::FINISHED;
}

};

int main() {

  Bar bar;
  auto func = std::make_shared<Task>(bar.foo(5));
  auto func2 = std::make_shared<Task>(bar.bar());
  func2->updateStatus();

  auto status = func2->status;

  TaskScheduler ts;

  ts.addTask(func);
  ts.RunMainThreadTasks();
  ts.runDatabaseThreadTasks();
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  ts.RunMainThreadTasks();
}