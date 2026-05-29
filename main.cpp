#include "./Generate_Orders/GenerateOrders.hpp"
#include "./Process_Orders/OrderPipeline.hpp"
#include "./Limit_Order_Book/Book.hpp"
#include "./Limit_Order_Book/Limit.hpp"
#include "./Limit_Order_Book/Order.hpp"
#include "./Limit_Order_Book/trade_logger.hpp"
#include <iostream>
#include <fstream>
#include <vector>
#include <chrono>

int main() {
    TradeLogger logger("docs/trades.csv");
    logger.start();

    Book* book = new Book(&logger);

    OrderPipeline orderPipeline(book);

    // GenerateOrders generateOrders(book);

    // generateOrders.createInitialOrders(10000, 300);

    {
        std::ifstream initialOrders("Generate_Orders/initialOrders.txt");
        if (!initialOrders) {
            std::cerr << "Error: Could not open initialOrders.txt" << std::endl;
            delete book;
            logger.stop();
            return 1;
        }
    }
    orderPipeline.processOrdersFromFile("Generate_Orders/initialOrders.txt");

    // generateOrders.createOrders(5000000);

    {
        std::ifstream orders("Orders.txt");
        if (!orders) {
            std::cerr << "Error: Could not open Orders.txt" << std::endl;
            delete book;
            logger.stop();
            return 1;
        }
    }

    // Start measuring time
    auto start = std::chrono::high_resolution_clock::now();

    orderPipeline.processOrdersFromFile("Orders.txt");

    // Stop measuring time
    auto stop = std::chrono::high_resolution_clock::now();

    // Calculate the duration
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(stop - start);

    std::cout << "Time taken to process orders: " << duration.count() << " microseconds" << std::endl;

    delete book;
    logger.stop();
    return 0;
}