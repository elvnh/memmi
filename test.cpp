#include <thread>
#include <cstdio>
#include <csignal>

static void thread_proc2()
{
    while (true) {
        printf("hello from child2\n");
    }
}

static void thread_proc()
{
    // std::thread t(thread_proc2);


    while (true) {
        printf("hello from child\n");
    }
}

int main()
{
    std::this_thread::sleep_for(std::chrono::seconds(3));

    std::thread t(thread_proc);

    while (true) {
        printf("hello from main\n");
    }

    return 0;
}
