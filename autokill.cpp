#include <chrono>
#include <iostream>
#include <optional>
#include <regex>
#include <string>
#include <string_view>
#include <thread>
#include <windows.h>

using namespace std::literals;

std::optional<std::string> getTitle(HWND window);
HANDLE getProcessHandle(HWND window);
BOOL CALLBACK enumerator(HWND window, LPARAM);
std::string getLastErrorString();

struct context
{
    struct window_info
    {
        std::string title;
        HWND window;
        HANDLE process;
    };

    std::regex pattern;
    std::chrono::steady_clock::time_point until;
    std::vector<window_info> windows;

    void operator()(HWND window)
    {
        auto title = getTitle(window);
        if(!title)
            return;

        auto process = getProcessHandle(window);
        if(!process)
            return;
        if(std::regex_search(*title, pattern))
            windows.push_back({std::move(*title), window, process});
    }

    template <typename F>
    void wait(F&& f)
    {
        std::this_thread::sleep_until(until);
        for(auto& w : windows)
        {
            f(w);
            CloseHandle(w.process);
        }
    }
};

int main(int argc, char** argv)
{
    auto usage_str =
        "Usage: "s + argv[0] +
        " title [seconds]\n"
        "Automatically kill all windows matching title after a delay, "
        "if provided.\n"
        "Title may be a ECMAScript regex.";

    if(argc != 2 && argc != 3)
    {
        std::cout << usage_str << '\n';
        return 1;
    }

    std::chrono::seconds delay{0};
    if(argc == 3)
    {
        delay = std::atoi(argv[2]) * 1s;
        if(delay == 0s && argv[2] != "0"s)
        {
            std::cout << usage_str << '\n';
            return 1;
        }
    }

    std::regex pattern{argv[1]};
    context ctx{pattern, std::chrono::steady_clock::now() + delay, {}};
    if(!EnumWindows(enumerator, (LPARAM)&ctx))
    {
        std::cout << "enumeration failed: " << getLastErrorString();
        return 1;
    }

    for(auto& w : ctx.windows)
    {
        std::cout << "killing \"" << w.title << "\"";
        if(delay > 0s)
            std::cout << " in " << delay.count() << " seconds";
        std::cout << '\n';
    }
    std::cout << std::flush;

    ctx.wait(
        [](auto&& w)
        {
            auto newtitle = getTitle(w.window);
            if(!TerminateProcess(w.process, 0))
            {
                std::cout << "cannot kill \"" << w.title
                          << "\": " << getLastErrorString();
                return;
            }

            if(newtitle && *newtitle != w.title)
                std::cout << "killed \"" << *newtitle << "\"(previously \""
                          << w.title << "\")\n";
            else
                std::cout << "killed \"" << w.title << "\"\n";
        });
}

std::optional<std::string> getTitle(HWND window)
{
    thread_local char buf[256];
    unsigned n = GetWindowText(window, buf, 256);
    if(n == 0)
        return {};
    return {{buf, n}};
}

HANDLE getProcessHandle(HWND window)
{
    DWORD pid;
    GetWindowThreadProcessId(window, &pid);
    return OpenProcess(PROCESS_TERMINATE, 1, pid);
}

BOOL CALLBACK enumerator(HWND window, LPARAM p)
{
    auto& ctx = *(context*)p;
    ctx(window);
    return 1;
}

std::string getLastErrorString()
{
    auto error = GetLastError();
    if(error == 0)
        return {};

    LPSTR buf = nullptr;
    size_t size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&buf,
        0, nullptr);

    std::string message(buf, size);
    LocalFree(buf);

    return message;
}