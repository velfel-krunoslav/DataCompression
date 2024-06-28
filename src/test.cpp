#include <iostream>
#include <map>
#include <functional>

int main() {
    // Create a map with keys sorted in descending order
    std::map<int, int, std::greater<int>> myMap = {{1, 40}, {2, 10}, {3, 20}, {7, 30}};

    // Insert elements into the map
    myMap[4] = 50;

    // Print the map
    for (const auto &pair : myMap) {
        std::cout << pair.first << ": " << pair.second << std::endl;
    }

    return 0;
}