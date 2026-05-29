#include <iostream>
#include <chrono>
#include <fstream>
#include "Limit_Order_Book/Book.hpp"
#include "Process_Orders/OrderPipeline.hpp"

int main() {
    Book* book = new Book();
    OrderPipeline orderPipeline(book);

    // 1. Safety check for initial orders
    std::ifstream initialCheck("Generate_Orders/initialOrders.txt");
    if (!initialCheck) {
        std::cerr << "Error: Could not open Generate_Orders/initialOrders.txt!" << std::endl;
        delete book;
        return 1;
    }
    initialCheck.close();

    // 2. Safety check for main orders
    std::ifstream ordersCheck("Orders.txt");
    if (!ordersCheck) {
        std::cerr << "Error: Could not open Orders.txt!" << std::endl;
        delete book;
        return 1;
    }
    ordersCheck.close();

    std::cout << "Files found. Starting Ultra-Low Latency Engine..." << std::endl;

    // Process initial state
    orderPipeline.processOrdersFromFile("Generate_Orders/initialOrders.txt");

    // Start High-Frequency Timer
    auto start = std::chrono::high_resolution_clock::now();

    // Process massive order file
    orderPipeline.processOrdersFromFile("Orders.txt");

    // Stop High-Frequency Timer
    auto stop = std::chrono::high_resolution_clock::now();

    // Calculate in MICROSECONDS
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(stop - start);
    
    std::cout << "Time taken to process orders: " << duration.count() << " microseconds" << std::endl;

    delete book;
    return 0;
}
