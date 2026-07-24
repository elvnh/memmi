#include <thread>
#include <cstdio>
#include <csignal>
#include <iostream>


#if defined(__x86_64__)
#    define DEBUG_BREAK __asm volatile("int3")
#else
#    error Unsupported architecture
#endif

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
    // int i = 0;
    // std::cout << (uintptr_t)&i << "\n";

    // std::this_thread::sleep_for(std::chrono::seconds(5));

    // i = 123;
    // std::cout << i;

    std::thread t(thread_proc);

    while (true) {
        printf("hello from main\n");
    }

    return 51;
}
