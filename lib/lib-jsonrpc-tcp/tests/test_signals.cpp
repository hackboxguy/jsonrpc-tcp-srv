// Signal handling (review finding H1): SIGTERM must reach the
// wait_for_exit_signal() loop even when the kernel delivers it to a busy
// worker thread, instead of killing the process with the default action.
#include "ADThread.hpp"
#include "ADTimer.hpp"
#include "adtest.hpp"
#include <signal.h>
#include <sys/wait.h>

namespace {
class Busy : public ADThreadConsumer {
public:
  ADThread *self;
  virtual int monoshot_callback_function(void *, ADThreadProducer *) {
    return 0;
  }
  virtual int thread_callback_function(void *, ADThreadProducer *) {
    unsigned long x = 0;
    while (!self->stop_requested()) // stays runnable: a likely signal target
      __atomic_add_fetch(&x, 1, __ATOMIC_RELAXED);
    return 0;
  }
};
// keeps the waiting thread busy outside sigwaitinfo(): while it waits, the
// kernel prefers it as the target, so the bug only shows when it is busy
class SlowSigio : public ADTimerConsumer {
public:
  virtual int timer_notification() { return 0; }
  virtual int sigio_notification() {
    usleep(300000);
    return 0;
  }
  virtual int custom_sig_notification(int) { return 0; }
};
void *killer(void *) {
  usleep(200000);
  kill(getpid(), SIGIO); // main thread now sleeps 300 ms in the subscriber
  usleep(100000);
  kill(getpid(), SIGTERM); // must not be lost or kill the process
  return NULL;
}
// runs in a child process: returns 0 if the shutdown path ran
int child_main() {
  ADTimer timer(100, -1);
  SlowSigio slow;
  timer.subscribe_timer_notification(&slow);
  Busy b[8];
  ADThread t[8];
  for (int i = 0; i < 8; i++) {
    b[i].self = &t[i];
    t[i].subscribe_thread_callback(&b[i]);
    t[i].set_thread_properties(THREAD_TYPE_NOBLOCK, NULL);
    t[i].start_thread();
  }
  pthread_t k;
  pthread_create(&k, NULL, killer, NULL);
  timer.wait_for_exit_signal();
  pthread_join(k, NULL);
  for (int i = 0; i < 8; i++)
    t[i].stop_thread();
  timer.stop_timer();
  return 0;
}
} // namespace

TEST_CASE("H1: SIGTERM with busy worker threads runs the shutdown path") {
  for (int round = 0; round < 10; round++) {
    pid_t pid = fork();
    REQUIRE(pid >= 0);
    if (pid == 0) {
      // exec a fresh copy: forking a threaded process is not supported by
      // the thread sanitizer runtime
      execl("/proc/self/exe", "test_signals", "--child", (char *)NULL);
      _exit(2);
    }
    int status = 0;
    REQUIRE(waitpid(pid, &status, 0) == pid);
    if (!(WIFEXITED(status) && WEXITSTATUS(status) == 42)) {
      printf("round %d: child %s %d\n", round,
             WIFSIGNALED(status) ? "killed by signal" : "exited with",
             WIFSIGNALED(status) ? WTERMSIG(status) : WEXITSTATUS(status));
      CHECK(false);
      return;
    }
  }
}

namespace {
// waits for the stop signal, then simulates a shutdown that hangs
int slow_shutdown_main() {
  ADTimer timer(100, -1);
  timer.wait_for_exit_signal();
  sleep(10);
  return 0;
}
// a write to a pipe without reader must not kill the process
int sigpipe_main() {
  ADTimer timer(100, -1);
  int p[2];
  if (pipe(p) != 0)
    return 1;
  close(p[0]);
  if (write(p[1], "x", 1) != -1 || errno != EPIPE)
    return 1;
  return 0;
}
pid_t spawn_child(const char *mode) {
  pid_t pid = fork();
  if (pid == 0) {
    execl("/proc/self/exe", "test_signals", mode, (char *)NULL);
    _exit(2);
  }
  return pid;
}
// waits up to timeout_ms for the child; returns its status or -1
int wait_child(pid_t pid, int timeout_ms) {
  int status = 0;
  for (int waited = 0; waited < timeout_ms; waited += 10) {
    if (waitpid(pid, &status, WNOHANG) == pid)
      return status;
    usleep(10000);
  }
  kill(pid, SIGKILL);
  waitpid(pid, &status, 0);
  return -1;
}
} // namespace

// V2-H3: after the first SIGTERM the shutdown may hang; a second SIGTERM
// must end the process instead of being swallowed
TEST_CASE("V2-H3: second SIGTERM ends a hanging shutdown") {
  pid_t pid = spawn_child("--slow-shutdown");
  REQUIRE(pid > 0);
  usleep(300000);
  kill(pid, SIGTERM);
  usleep(300000);
  kill(pid, SIGTERM);
  int status = wait_child(pid, 3000);
  REQUIRE(status != -1); // still running after 3 s: second signal ignored
  CHECK(WIFSIGNALED(status) && WTERMSIG(status) == SIGTERM);
}

TEST_CASE("V2-H3: SIGPIPE does not kill the service") {
  pid_t pid = spawn_child("--sigpipe");
  REQUIRE(pid > 0);
  int status = wait_child(pid, 3000);
  REQUIRE(status != -1);
  CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 42);
}

int main(int argc, char **argv) {
  if (argc > 1 && strcmp(argv[1], "--child") == 0)
    return child_main() == 0 ? 42 : 1;
  if (argc > 1 && strcmp(argv[1], "--slow-shutdown") == 0)
    return slow_shutdown_main() == 0 ? 42 : 1;
  if (argc > 1 && strcmp(argv[1], "--sigpipe") == 0)
    return sigpipe_main() == 0 ? 42 : 1;
  return adtest::run_all(argc, argv);
}
