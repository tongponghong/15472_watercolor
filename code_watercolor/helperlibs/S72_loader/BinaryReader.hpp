#include <iostream> 
#include <fstream>
#include <cstring> 


void get_binary_data(std::string filepath) {
    std::ifstream file(filepath, std::ios::in | std::ios::binary);

    if (file.is_open()) {
        std::ifstream::pos_type size = file.tellg();
        char *buffer = new char [size];
        file.seekg (0, std::ios::beg);
        file.read(buffer, size);
        file.close();

        std::cout << "copied to buffer" << std::endl;
        
        delete[] buffer;
    }

    else std::cout << "File wasn't read" << std::endl;
    return 0;
}


