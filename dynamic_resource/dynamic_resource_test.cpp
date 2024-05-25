#include <gtest/gtest.h>
#include <fstream>
#include "dynamic_resource.h"

TEST(DynamicResourceTest, LoadFile) {
    struct Dynamic_config {
        using value = std::string;
        Dynamic_config(std::filesystem::path path) {
            std::ifstream infile{path};
            std::stringstream file_stream;
            file_stream << infile.rdbuf();
            important_data = file_stream.str();
        }
        const std::string& get_data()const { return important_data; }
        std::string important_data{""};
    };
    //std::filesystem::path path{ "C:\\TestingFolder" }; //creates TestingFolder object on C:
    //path /= "my new file.txt"; //put something into there
    //std::filesystem::create_directories(path.parent_path()); //add directories based on the object path (without this line it will not work)
    std::ofstream out("test_load_file.txt");
    out << "static msg\n";
    out.close(); // unnessasary in the end of the scope only if need to check if close was successful
    //auto test_res = Dynamic_config("test_load_file.txt");
    Dynamic_resource::Dynamic_resource<Dynamic_config> res{"test_load_file.txt"};
    EXPECT_EQ(res.get_data(),  "static msg\n");
}

int main(int argc, char **argv){
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
