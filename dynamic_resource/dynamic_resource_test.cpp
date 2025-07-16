#include <gtest/gtest.h>
#include <fstream>
#include "dynamic_resource.h"


using namespace std::literals;
constexpr std::chrono::milliseconds WAIT_TIME_FOR_UPDATE_MS = 6000ms;

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
    std::cerr << sizeof(res) << "\n";
    EXPECT_EQ(res.get_data(),  "static msg\n");
}

// load file and the update in the runtime, check for new content
TEST(DynamicResourceTest, LoadFileAndUpdate) {
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

    std::ofstream out("test_load_file_and_update.txt");
    out << "static msg";
    out.close();
    std::this_thread::sleep_for(300ms);
    Dynamic_resource::Dynamic_resource<Dynamic_config> res{"test_load_file_and_update.txt"};
    std::cerr << sizeof(res) << "\n";
    std::ofstream out2("test_load_file_and_update.txt", std::ios::app);
    EXPECT_EQ(res.get_data(),  "static msg");
    out2 << " + addded msg\n";
    out2.close();
    std::this_thread::sleep_for(Dynamic_resource::UPDATE_RESOURCE_EVERY_X_MS + WAIT_TIME_FOR_UPDATE_MS);
    EXPECT_EQ(res.get_data(),  "static msg + addded msg\n");
}

// use old res version if resource deleted for some reason
TEST(DynamicResourceTest, LoadFileAndDelete) {
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

    std::ofstream out("test_load_file_and_delete.txt");
    out << "static msg";
    out.close();
    std::this_thread::sleep_for(300ms);
    Dynamic_resource::Dynamic_resource<Dynamic_config> res{"test_load_file_and_delete.txt"};
    std::cerr << sizeof(res) << "\n";
    EXPECT_EQ(res.get_data(),  "static msg");
    std::remove("test_load_file_and_delete.txt");
    std::this_thread::sleep_for(Dynamic_resource::UPDATE_RESOURCE_EVERY_X_MS + WAIT_TIME_FOR_UPDATE_MS);
    EXPECT_EQ(res.get_data(),  "static msg");
}

int main(int argc, char **argv){
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
