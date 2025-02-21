#include <iostream>
#include <chrono>
#include <ctime>
#include "SHA256.h"
#include <functional> 
#include <iomanip>   // Дstd::setw и std::setfill
#include <fstream>   
#include <thread>    
#include <mutex>     
#include <future>    // Для асинхронных задач
#include <atomic>    
#include <vector>    // Для хранения потоков
#include <queue>     // Для очереди задач
#include <condition_variable> // Для синхронизации потоков
#include <filesystem> 

constexpr size_t TICKET_LENGTH = 9;
constexpr size_t SALT_LENGTH = 36;

std::mutex mtx; // Мьютекс для синхронизации вывода и записи в файл
std::ofstream logFile; // Файл для логирования результатов
std::atomic<int> processedCombinations{0}; // Счетчик обработанных комбинаций

// Пул потоков
class ThreadPool {
public:
    ThreadPool(size_t threads) : stop(false) {
        for (size_t i = 0; i < threads; ++i)
            workers.emplace_back([this] {
                for (;;) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(this->queue_mutex);
                        this->condition.wait(lock, [this] { return this->stop || !this->tasks.empty(); });
                        if (this->stop && this->tasks.empty())
                            return;
                        task = std::move(this->tasks.front());
                        this->tasks.pop();
                    }
                    task();
                }
            });
    }

    template<class F>
    void enqueue(F&& f) {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            tasks.emplace(std::forward<F>(f));
        }
        condition.notify_one();
    }

    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            stop = true;
        }
        condition.notify_all();
        for (std::thread &worker : workers)
            worker.join();
    }

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;

    std::mutex queue_mutex;
    std::condition_variable condition;
    bool stop;
};

// Для генерации билета (9 цифр)
void generateTickets(const std::function<void(const std::string&)>& callback) {
    for (int i = 0; i <= 999999999; ++i) {
        std::string ticket = std::to_string(i);
        // Дополняем строку нулями до 9 символов
        ticket.insert(0, TICKET_LENGTH - ticket.length(), '0');
        callback(ticket);
    }
}

// Для генерации соли (36 символов)
void generateSalt(const std::function<void(const std::string&)>& callback) {
    const std::string chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    std::string salt(SALT_LENGTH, ' ');

    // Итеративная функция для генерации всех комбинаций
    size_t indices[SALT_LENGTH] = {0};
    while (true) {
        for (size_t i = 0; i < SALT_LENGTH; ++i) {
            salt[i] = chars[indices[i]];
        }
        callback(salt);

        // Переход к следующей комбинации
        size_t pos = SALT_LENGTH - 1;
        while (pos != size_t(-1) && ++indices[pos] == chars.size()) {
            indices[pos] = 0;
            --pos;
        }
        if (pos == size_t(-1)) break;
    }
}

// Функция для хеширования строки и вывода результата
void hashAndPrint(const std::string& input) {
    SHA256 sha;
    sha.update(input);
    std::array<uint8_t, 32> digest = sha.digest();

    // Синхронизированный вывод и запись в файл
    std::lock_guard<std::mutex> lock(mtx);
    std::cout << "SHA-256: " << SHA256::toString(digest) << std::endl;
    if (logFile.is_open()) {
        logFile << "SHA-256: " << SHA256::toString(digest) << std::endl;
    }
}

void bruteforceMode() {
    std::cout << "Начало перебора комбинаций..." << std::endl;

    // Открываем файл для логирования
    logFile.open("hashes.log");
    if (!logFile.is_open()) {
        std::cerr << "Ошибка открытия файла для логирования!" << std::endl;
        return;
    }

    // Создаем пул потоков
    ThreadPool pool(std::thread::hardware_concurrency());

    // Запускаем асинхронные задачи для генерации билетов
    auto startTime = std::chrono::high_resolution_clock::now();

    generateTickets([&](const std::string& ticket) {
        pool.enqueue([&ticket]() {
            generateSalt([&ticket](const std::string& salt) {
                std::string input = ticket + salt;

                hashAndPrint(input);

                processedCombinations++;

                // if (processedCombinations % 1000 == 0) {
                //     std::lock_guard<std::mutex> lock(mtx);
                //     auto now = std::chrono::high_resolution_clock::now();
                //     auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();
                //     std::cout << "Обработано комбинаций: " << processedCombinations
                //                << ", Затраченное время: " << elapsed << " сек" << std::endl;
                // }
            });
        });
    });

    // Закрываем файл
    logFile.close();

    auto endTime = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(endTime - startTime).count();
    std::cout << "Перебор завершен. Общее время: " << elapsed << " сек" << std::endl;
}

// Режим ввода одной строки
void interactiveMode() {
    std::string input;
    std::cout << "Введите строку для хэширования: ";
    while (std::getline(std::cin, input)) {
        if (input.empty()) {
            break;         }

        hashAndPrint(input);

        std::cout << "Введите следующую строку (или нажмите Enter для выхода): ";
    }
}

int main(int argc, char** argv) {
    // Режим перебора всех комбинаций
    if (argc > 1 && std::string(argv[1]) == "--bruteforce") {
        bruteforceMode();
    }
    // Режим ввода одной строки
    else {
        interactiveMode();
    }

    return EXIT_SUCCESS;
}